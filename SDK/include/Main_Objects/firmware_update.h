#ifndef FIRMWARE_UPDATE_H
#define FIRMWARE_UPDATE_H

#include <stddef.h>
#include <stdio.h>

#include <anjay/fw_update.h>
#include <anjay/download.h>

typedef struct {
    char *administratively_set_target_path;
    char *next_target_path;
    char *package_uri;
    char *persistence_file;
    FILE *firmware_update_stream;
    char **startup_args;
    avs_net_security_info_t security_info;
} firmware_update_logic_t;

int firmware_update_install(anjay_t *anjay,
                            firmware_update_logic_t *fw_update,
                            const char *persistence_file,
                            const avs_net_security_info_t *security_info,
                            const avs_coap_tx_params_t *tx_params,
                            const char *const *startup_args);

void firmware_update_destroy(firmware_update_logic_t *fw_update);

void firmware_update_set_package_path(firmware_update_logic_t *fw_update,
                                      const char *file_path);


#endif // FIRMWARE_UPDATE_H
