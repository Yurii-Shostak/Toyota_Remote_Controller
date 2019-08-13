#define  _BSD_SOURCE
#include "firmware_update.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stdlib.h"
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <avsystem/commons/persistence.h>
#include <avsystem/commons/memory.h>
#include <avsystem/commons/stream/stream_file.h>
#include <avsystem/commons/utils.h>
#include <anjay/anjay.h>
#include <anjay/dm.h>
#include <toyota_client.h>
#include <toyota_utils.h>

#define FORCE_ERROR_OUT_OF_MEMORY 1
#define FORCE_ERROR_FAILED_UPDATE 2

#define FIRMWARE_UPDATE_PACKAGE_NAME        "Toyota_FW"                // name of firmware update package
#define FIRMWARE_UPDATE_PACKAGE_VERSION     "1.0"                      // version of firmware update package
#define FIRMWARE_UPDATE_RANDOM_FILE_PATH    "/tmp/toyota_fw-XXXXXX"    // random file path for firmware update process

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define firmware_log(level, ...) avs_log(toyota_fw, level, __VA_ARGS__)

static int
open_temporary_file(char *path) {
    mode_t old_umask = (mode_t) umask(S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH
                                      | S_IWOTH | S_IXOTH);
    int fd = mkstemp(path);
    umask(old_umask);
    return fd;
}

static char*
generate_random_target_filepath(void) {
    char *result = NULL;
    if (!(result = avs_strdup(FIRMWARE_UPDATE_RANDOM_FILE_PATH))) {
        firmware_log(ERROR,"out of memory");
        return NULL;
    }

    int fd = open_temporary_file(result);
    if (fd == -1) {
        firmware_log(ERROR, "could not generate filename: %s",
                 strerror(errno));
        avs_free(result);
        return NULL;
    }
    firmware_log(DEBUG, "successfully generated firmware target filepath");
    close(fd);
    return result;
}

static int
maybe_create_firmware_file(firmware_update_logic_t *fw_update) {
    if (!fw_update->next_target_path) {
        if (fw_update->administratively_set_target_path) {
            fw_update->next_target_path =
                    avs_strdup(fw_update->administratively_set_target_path);
        } else {
            fw_update->next_target_path = generate_random_target_filepath();
        }
        if (!fw_update->next_target_path) {
            firmware_log(ERROR, "could not create file");
            return -1;
        }
        firmware_log(INFO, "created %s", fw_update->next_target_path);
    }
    return 0;
}

static void
maybe_delete_firmware_file(firmware_update_logic_t *fw_update) {
    if (fw_update->next_target_path) {
        unlink(fw_update->next_target_path);
        firmware_log(INFO, "deleted %s", fw_update->next_target_path);
        avs_free(fw_update->next_target_path);
        fw_update->next_target_path = NULL;
    }
}

void
firmware_update_set_package_path(firmware_update_logic_t *fw_update,
                                 const char *file_path) {
    AVS_ASSERT(!fw_update->firmware_update_stream,
               "cannot set package path while a downloading in progress");
    char *new_target_path = avs_strdup(file_path);
    if (!new_target_path) {
        firmware_log(ERROR, "out of memory");
        return;
    }

    avs_free(fw_update->administratively_set_target_path);
    fw_update->administratively_set_target_path = new_target_path;
    firmware_log(INFO, "firmware package path set to %s",
                 fw_update->administratively_set_target_path);
}

static int
copy_file_content(FILE *destination, FILE *source) {
    while (!feof(source)) {
        char buffer[4096];

        size_t bytes_read = fread(buffer, 1, sizeof(buffer), source);
        if (bytes_read == 0 && ferror(source)) {
            firmware_log(ERROR, "could not read data from source file");
            return -1;
        }

        if (fwrite(buffer, 1, bytes_read, destination) != bytes_read) {
            firmware_log(ERROR, "could not write data to destination file");
            return -1;
        }
    }

    firmware_log(INFO, "successfully copy content from source file to destination file");
    return 0;
}

static int
unpack_firmware_to_file(const char *fw_package_path,
                        const char *target_path) {
    int result = -1;
    FILE *firmware = fopen(fw_package_path, "rb");
    FILE *temporary = NULL;

    if (!firmware) {
        firmware_log(ERROR, "could not open file %s", fw_package_path);
        goto cleanup;
    }

    temporary = fopen(target_path, "wb");
    if (!temporary) {
        firmware_log(ERROR, "could not open file: %s", target_path);
        goto cleanup;
    }
    result = copy_file_content(temporary, firmware);
    if (result) {
        firmware_log(ERROR, "could not copy firmware from %s to %s", fw_package_path,
                     target_path);
        goto cleanup;
    }

    result = 0;

cleanup:
    if (firmware) {
        firmware_log(DEBUG, "close firmware file");
        fclose(firmware);
    }
    if (temporary) {
        firmware_log(DEBUG, "close temporary file");
        fclose(temporary);
    }
    return result;
}

static int
unpack_firmware_in_place(firmware_update_logic_t *firmware_update) {
    char *temporary_path = generate_random_target_filepath();
    if (!temporary_path) {
        firmware_log(ERROR, "could not generate temporary file to unpack firmware");
        return -1;
    }

    int result =
            unpack_firmware_to_file(firmware_update->next_target_path,
                                    temporary_path);
    if (result) {
        goto cleanup;
    }

    if ((result = rename(temporary_path, firmware_update->next_target_path)) ==-1) {
        firmware_log(ERROR, "сould not rename %s to %s: %s", temporary_path,
                     firmware_update->next_target_path, strerror(errno));
        goto cleanup;
    }
    if ((result = chmod(firmware_update->next_target_path, 0700)) == -1) {
        firmware_log(ERROR, "сould not set permissions for %s: %s",
                     firmware_update->next_target_path, strerror(errno));
        goto cleanup;
    }

cleanup:
    unlink(temporary_path);
    avs_free(temporary_path);
    if (result) {
        maybe_delete_firmware_file(firmware_update);
    }

    return result;
}

static int preprocess_firmware(firmware_update_logic_t *fw_update) {
    if (unpack_firmware_in_place(fw_update)) {
        return ANJAY_FW_UPDATE_ERR_UNSUPPORTED_PACKAGE_TYPE;
    }

    firmware_log(INFO, "firmware downloaded successfully");
    return 0;
}

static int store_etag(avs_persistence_context_t *ctx,
                      const anjay_etag_t *etag) {
    // UINT16_MAX is a magic value that means "there is no ETag"
    uint16_t size16 = (etag ? etag->size : UINT16_MAX);
    int result = avs_persistence_u16(ctx, &size16);
    if (!result && etag) {
        result = avs_persistence_bytes(ctx, (uint8_t *) (intptr_t) etag->value,
                                       etag->size);
    }
    return result;
}

static int write_persistence_file(const char *path,
                                  anjay_fw_update_initial_result_t result,
                                  const char *uri,
                                  char *download_file,
                                  bool filename_administratively_set,
                                  const anjay_etag_t *etag) {
    avs_stream_abstract_t *stream = NULL;
    avs_persistence_context_t *ctx = NULL;
    int8_t result8 = (int8_t) result;
    int retval = 0;
    if (!(stream = avs_stream_file_create(path, AVS_STREAM_FILE_WRITE))
            || !(ctx = avs_persistence_store_context_new(stream))
            || avs_persistence_bytes(ctx, (uint8_t *) &result8, 1)
            || avs_persistence_string(ctx, (char **) (intptr_t) &uri)
            || avs_persistence_string(ctx, &download_file)
            || avs_persistence_bool(ctx, &filename_administratively_set)
            || store_etag(ctx, etag)) {
        firmware_log(ERROR, "could not write firmware state persistence file");
        retval = -1;
    }
    if (ctx) {
        avs_persistence_context_delete(ctx);
    }
    if (stream) {
        avs_stream_cleanup(&stream);
    }
    if (retval) {
        unlink(path);
    }
    return retval;
}

static void delete_persistence_file(const firmware_update_logic_t *fw_update) {
    firmware_log(DEBUG, "delete persistence file");
    unlink(fw_update->persistence_file);
}

static void fw_reset(void *fw_) {
    firmware_log(DEBUG, "reset firmware update process");
    firmware_update_logic_t *fw_update = (firmware_update_logic_t *) fw_;
    if (fw_update->firmware_update_stream) {
        fclose(fw_update->firmware_update_stream);
        fw_update->firmware_update_stream = NULL;
    }
    avs_free(fw_update->package_uri);
    fw_update->package_uri = NULL;
    maybe_delete_firmware_file(fw_update);
    delete_persistence_file(fw_update);
}

static int fw_stream_open(void *fw_,
                          const char *package_uri,
                          const struct anjay_etag *package_etag) {
    (void) package_uri;
    (void) package_etag;
    firmware_update_logic_t *fw_update = (firmware_update_logic_t *) fw_;
    assert(!fw_update->firmware_update_stream);
    firmware_log(INFO, "open firmware update stream");
    char *uri = NULL;
    if (package_uri && !(uri = avs_strdup(package_uri))) {
        firmware_log(ERROR, "out of memory");
        return -1;
    }

    if (maybe_create_firmware_file(fw_update)) {
        avs_free(uri);
        return -1;
    }

    if (!(fw_update->firmware_update_stream = fopen(fw_update->next_target_path, "wb"))) {
        firmware_log(ERROR, "could not open file: %s", fw_update->next_target_path);
        avs_free(uri);
        return -1;
    }

    avs_free(fw_update->package_uri);
    fw_update->package_uri = uri;
    if (write_persistence_file(
                fw_update->persistence_file,
                ANJAY_FW_UPDATE_INITIAL_DOWNLOADING,
                package_uri, fw_update->next_target_path,
                !!fw_update->administratively_set_target_path, package_etag)) {
        fw_reset(fw_);
        return -1;
    }

    return 0;
}

static int fw_stream_write(void *fw_, const void *data, size_t length) {
    firmware_update_logic_t *fw_update = (firmware_update_logic_t *) fw_;
    if (!fw_update->firmware_update_stream) {
        firmware_log(ERROR, "stream not open");
        return -1;
    }
    if (length
            && (fwrite(data, length, 1, fw_update->firmware_update_stream) != 1
                // Firmware update integration tests measure download
                // progress by checking file size, so avoiding buffering
                // is required.
                || fflush(fw_update->firmware_update_stream) != 0)) {
        firmware_log(ERROR, "fwrite or fflush failed: %s", strerror(errno));
        return ANJAY_FW_UPDATE_ERR_NOT_ENOUGH_SPACE;
    }

    return 0;
}

static int fw_stream_finish(void *fw_) {
    firmware_update_logic_t *fw_update = (firmware_update_logic_t *) fw_;
    if (!fw_update->firmware_update_stream) {
        firmware_log(ERROR, "stream not open");
        return -1;
    }
    fclose(fw_update->firmware_update_stream);
    fw_update->firmware_update_stream = NULL;

    int result = 0;
    if ((result = preprocess_firmware(fw_update))
            || (result = write_persistence_file(
                        fw_update->persistence_file,
                        ANJAY_FW_UPDATE_INITIAL_DOWNLOADED, fw_update->package_uri,
                        fw_update->next_target_path,
                        !!fw_update->administratively_set_target_path, NULL))) {
        fw_reset(fw_update);
    }
    firmware_log(DEBUG, "firmware stream finished");
    return result;
}

static const char *fw_get_name(void *fw) {
    (void) fw;
    return FIRMWARE_UPDATE_PACKAGE_NAME;
}

static const char *fw_get_version(void *fw) {
    (void) fw;
    return FIRMWARE_UPDATE_PACKAGE_VERSION;
}

static int fw_perform_upgrade(void *fw_) {
    firmware_update_logic_t *fw_update = (firmware_update_logic_t *) fw_;
    if (write_persistence_file(fw_update->persistence_file,
                               ANJAY_FW_UPDATE_INITIAL_SUCCESS, NULL,
                               fw_update->next_target_path,
                               !!fw_update->administratively_set_target_path, NULL)) {
        delete_persistence_file(fw_update);
        return -1;
    }

    firmware_log(INFO, "|| =========== FIRMWARE UPDATE STARTED: %s =========== ||", fw_update->next_target_path);
    execv(fw_update->next_target_path, fw_update->startup_args);
    firmware_log(ERROR, "execv failed (%s)", strerror(errno));
    delete_persistence_file(fw_update);
    return -1;
}

static int fw_get_security_info(void *fw_,
                                avs_net_security_info_t *out_security_info,
                                const char *download_uri) {
    firmware_update_logic_t *fw_update = (firmware_update_logic_t *) fw_;
    (void) download_uri;
    memcpy(out_security_info, &fw_update->security_info, sizeof(fw_update->security_info));
    return 0;
}

static avs_coap_tx_params_t g_tx_params;

static avs_coap_tx_params_t fw_get_coap_tx_params(void *user_ptr,
                                                  const char *download_uri) {
    (void) user_ptr;
    (void) download_uri;
    return g_tx_params;
}

static anjay_fw_update_handlers_t FW_UPDATE_HANDLERS = {
    .stream_open = fw_stream_open,
    .stream_write = fw_stream_write,
    .stream_finish = fw_stream_finish,
    .reset = fw_reset,
    .get_name = fw_get_name,
    .get_version = fw_get_version,
    .perform_upgrade = fw_perform_upgrade,
    .get_coap_tx_params = fw_get_coap_tx_params
};

static int restore_etag(avs_persistence_context_t *ctx, anjay_etag_t **etag) {
    assert(etag && !*etag);
    uint16_t size16;
    int result = avs_persistence_u16(ctx, &size16);
    if (!result && size16 <= UINT8_MAX) {
        *etag = (anjay_etag_t *) avs_malloc(offsetof(anjay_etag_t, value)
                                            + size16);
        if (!*etag) {
            return -1;
        }
        (*etag)->size = (uint8_t) size16;
        if ((result = avs_persistence_bytes(ctx, (*etag)->value, size16))) {
            avs_free(*etag);
            *etag = NULL;
        }
    }
    return result;
}

static bool is_valid_result(int8_t result) {
    switch (result) {
    case ANJAY_FW_UPDATE_INITIAL_DOWNLOADED:
    case ANJAY_FW_UPDATE_INITIAL_DOWNLOADING:
    case ANJAY_FW_UPDATE_INITIAL_NEUTRAL:
    case ANJAY_FW_UPDATE_INITIAL_SUCCESS:
    case ANJAY_FW_UPDATE_INITIAL_INTEGRITY_FAILURE:
    case ANJAY_FW_UPDATE_INITIAL_FAILED:
        return true;
    default:
        return false;
    }
}

typedef struct {
    anjay_fw_update_initial_result_t result;
    char *uri;
    char *download_file;
    bool filename_administratively_set;
    anjay_etag_t *etag;
} persistence_file_data_t;

static persistence_file_data_t read_persistence_file(const char *path) {
    persistence_file_data_t data;
    memset(&data, 0, sizeof(data));
    avs_stream_abstract_t *stream = NULL;
    avs_persistence_context_t *ctx = NULL;
    int8_t result8 = (int8_t) ANJAY_FW_UPDATE_INITIAL_NEUTRAL;
    if ((stream = avs_stream_file_create(path, AVS_STREAM_FILE_READ))) {
        // invalid or empty but existing file still signifies success
        result8 = (int8_t) ANJAY_FW_UPDATE_INITIAL_SUCCESS;
    }
    if (!stream || !(ctx = avs_persistence_restore_context_new(stream))
            || avs_persistence_bytes(ctx, (uint8_t *) &result8, 1)
            || !is_valid_result(result8)
            || avs_persistence_string(ctx, &data.uri)
            || avs_persistence_string(ctx, &data.download_file)
            || avs_persistence_bool(ctx, &data.filename_administratively_set)
            || restore_etag(ctx, &data.etag)) {
        firmware_log(WARNING,
                     "invalid data in the firmware state persistence file");
        avs_free(data.uri);
        avs_free(data.download_file);
        memset(&data, 0, sizeof(data));
    }
    data.result = (anjay_fw_update_initial_result_t) result8;
    if (ctx) {
        avs_persistence_context_delete(ctx);
    }
    if (stream) {
        avs_stream_cleanup(&stream);
    }
    return data;
}

static void argv_free(char **argv) {
    if (!argv) {
        return;
    }
    for (int i = 0; argv[i]; ++i) {
        avs_free(argv[i]);
    }
    avs_free(argv);
}

static char** argv_copy(const char *const *argv) {
    if (!argv) {
        return NULL;
    }

    int argc = 0;
    while (argv[argc]) {
        ++argc;
    }

    char **copy = avs_calloc(argc + 1, sizeof(char*));
    if (!copy) {
        return NULL;
    }

    for (int i = 0; i < argc; ++i) {
        copy[i] = avs_strdup(argv[i]);
        if (!copy[i]) {
            argv_free(copy);
            return NULL;
        }
    }

    return copy;
}

int firmware_update_install(anjay_t *anjay,
                            firmware_update_logic_t *fw_update,
                            const char *persistence_file,
                            const avs_net_security_info_t *security_info,
                            const avs_coap_tx_params_t *tx_params,
                            const char *const *startup_args) {
    fw_update->startup_args = argv_copy(startup_args);
    if (!fw_update->startup_args) {
        firmware_log(ERROR, "out of memory");
        return -1;
    }

    fw_update->persistence_file = avs_strdup(persistence_file);
    if (!fw_update->persistence_file) {
        firmware_log(ERROR, "out of memory");
        return -1;
    }
    firmware_log(DEBUG, "persistence file: %s", persistence_file);
    if (security_info) {
        memcpy(&fw_update->security_info, security_info, sizeof(fw_update->security_info));
        FW_UPDATE_HANDLERS.get_security_info = fw_get_security_info;
    } else {
        FW_UPDATE_HANDLERS.get_security_info = NULL;
    }

    if (tx_params) {
        g_tx_params = *tx_params;
        FW_UPDATE_HANDLERS.get_coap_tx_params = fw_get_coap_tx_params;
    } else {
        FW_UPDATE_HANDLERS.get_coap_tx_params = NULL;
    }

    persistence_file_data_t data = read_persistence_file(persistence_file);
    delete_persistence_file(fw_update);
    firmware_log(INFO, "initial firmware upgrade state result: %d",
                 (int) data.result);
    if ((fw_update->next_target_path = data.download_file)
            && data.filename_administratively_set
            && !(fw_update->administratively_set_target_path =
                         avs_strdup(data.download_file))) {
        firmware_log(WARNING, "could not administratively set firmware path");
    }
    anjay_fw_update_initial_state_t state = {
        .result = data.result,
        .persisted_uri = data.uri,
        .resume_offset = 0,
        .resume_etag = data.etag
    };

    if (state.result == ANJAY_FW_UPDATE_INITIAL_DOWNLOADING) {
        long offset;
        if (!fw_update->next_target_path
                || !(fw_update->firmware_update_stream = fopen(fw_update->next_target_path, "ab"))
                || (offset = ftell(fw_update->firmware_update_stream)) < 0) {
            if (fw_update->firmware_update_stream) {
                fclose(fw_update->firmware_update_stream);
                fw_update->firmware_update_stream = NULL;
            }
            state.result = ANJAY_FW_UPDATE_INITIAL_NEUTRAL;
        } else {
            state.resume_offset = (size_t) offset;
        }
    }
    if (state.result >= 0) {
        // we're initializing in the "Idle" state, so the firmware file is not
        // supposed to exist; delete it if we have it for any weird reason
        maybe_delete_firmware_file(fw_update);
    }

    int result =
            anjay_fw_update_install(anjay, &FW_UPDATE_HANDLERS, fw_update, &state);
    avs_free(data.uri);
    avs_free(data.etag);
    if (result) {
        firmware_update_destroy(fw_update);
    }
    return result;
}

void firmware_update_destroy(firmware_update_logic_t *fw_update) {
    firmware_log(ERROR, "destroy firmware update");
    if (fw_update->firmware_update_stream) {
        fclose(fw_update->firmware_update_stream);
    }
    avs_free(fw_update->package_uri);
    avs_free(fw_update->administratively_set_target_path);
    avs_free(fw_update->next_target_path);
    avs_free(fw_update->persistence_file);
    argv_free(fw_update->startup_args);
}

