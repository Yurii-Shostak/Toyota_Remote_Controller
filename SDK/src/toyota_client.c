#include "toyota_client.h"

#include "assert.h"
#include "signal.h"
#include "stdio.h"
#include "time.h"
#include "poll.h"

#include <avsystem/commons/log.h>
#include <avsystem/commons/defs.h>
#include <anjay/anjay.h>
#include <anjay/security.h>
#include <anjay/server.h>
#include <anjay/attr_storage.h>

#include "Main_Objects/humidity.h"
#include "Main_Objects/firmware_update.h"
#include "Main_Objects/headlights_control.h"

#define INPUT_BUFFER_SIZE  10000
#define OUTPUT_BUFFER_SIZE 10000
#define DEFAULT_MIN_PERIOD -1
#define DEFAULT_MAX_PERIOD -1
#define DISABLE_TIMEOUT    -1

struct client {
    anjay_t *anjay;                                   // main lwm2m context
    firmware_update_logic_t  firmware_update;         // main structure of firmware_update object
    const char               *fw_updated_marker_path; // firmware update marker filepath
};

void 
remote_client_poll_sockets(client_t *self, int max_wait_time_ms) {
    
    anjay_t *anjay = self->anjay;

    // Obtain all network data sources
    AVS_LIST(avs_net_abstract_socket_t *const) sockets = anjay_get_sockets(anjay);

    // Prepare to poll() on them
    size_t numsocks = AVS_LIST_SIZE(sockets);
    struct pollfd pollfds[numsocks];
    size_t i = 0;

    AVS_LIST(avs_net_abstract_socket_t *const) sock;
    AVS_LIST_FOREACH(sock, sockets) {
        pollfds[i].fd = *(const int *) avs_net_socket_get_system(*sock);
        pollfds[i].events = POLLIN;
        pollfds[i].revents = 0;
        ++i;
    }

    // Determine the expected time to the next job in milliseconds.
    // If there is no job we will wait till something arrives for
    // at most 1 second (i.e. max_wait_time_ms).
    int wait_ms = anjay_sched_calculate_wait_time_ms(anjay, max_wait_time_ms);

    // Wait for the events if necessary, and handle them.
    if (poll(pollfds, numsocks, wait_ms) > 0) {
        int socket_id = 0;
        AVS_LIST(avs_net_abstract_socket_t *const) socket = NULL;
        AVS_LIST_FOREACH(socket, sockets) {
            if (pollfds[socket_id].revents) {
                if (anjay_serve(anjay, *socket)) {
                    avs_log(toyota_client, ERROR, "anjay_serve failed");

                }
            }
            ++socket_id;
        }
    }

    if (anjay_all_connections_failed(anjay)) {
        avs_log(toyota_client, ERROR, "All connections failed, trying to reconnect...");
        anjay_schedule_reconnect(anjay);
    }


    // Finally run the scheduler (ignoring its return value, which
    // is the number of tasks executed)
    (void) anjay_sched_run(anjay);

}

client_t *
remote_client_create(uint16_t          ssid,
                     const char        *endpoint_name,
                     const char        *server_uri,
                     const char        *binding_mode,
                     int               lifetime,
                     bool              bootstrap_state,
                     const char        *fw_updated_marker_path,
                     const char *const *fw_update_args) {

    assert(endpoint_name);
    assert(server_uri);
    assert(binding_mode);
    assert(lifetime > 0);

    const char PSK_IDENTITY[] = "yurii.shostak";          // default PSK identity
    const char PSK_KEY[]      = "18041994yayura18041994"; // default PSK key

    client_t *client = NULL;                              // main lwm2m client pointer 
    anjay_t  *anjay  = NULL;                              // main anjay-object pointer
    
    // setup main cinfiguration
    anjay_configuration_t connection_config = {
        .endpoint_name             = endpoint_name,
        .in_buffer_size            = INPUT_BUFFER_SIZE,
        .out_buffer_size           = OUTPUT_BUFFER_SIZE,
        .dtls_version              = AVS_NET_SSL_VERSION_TLSv1_2,
        .confirmable_notifications = true,
    };

    anjay = anjay_new(&connection_config);
    if (!anjay) {
        log_error(toyota_client, "Could not create Anjay object");
        goto error;
    }

    if (anjay_attr_storage_install(anjay)
            || anjay_security_object_install(anjay)
            || anjay_server_object_install(anjay)) {
        log_error(toyota_client, "Could not install required object(s)");
        goto error;
    }
    
    anjay_security_instance_t security_instance = {
        .ssid                             = ssid,
        .bootstrap_server                 = bootstrap_state,
        .server_uri                       = server_uri,
        .security_mode                    = ANJAY_UDP_SECURITY_PSK,
        .public_cert_or_psk_identity      = (const uint8_t *) PSK_IDENTITY,
        .public_cert_or_psk_identity_size = strlen(PSK_IDENTITY),
        .private_cert_or_psk_key          = (const uint8_t *) PSK_KEY,
        .private_cert_or_psk_key_size     = strlen(PSK_KEY),
    };

    anjay_iid_t security_instance_id = ANJAY_IID_INVALID;
    if (anjay_security_object_add_instance(anjay, &security_instance,
            &security_instance_id)) {
        log_error(toyota_client, "Could not add security instance");
        goto error;
    }

    // setup server instance
    anjay_server_instance_t server_instance = {
        .ssid               = ssid,
        .lifetime           = lifetime,
        .default_min_period = DEFAULT_MIN_PERIOD,
        .default_max_period = DEFAULT_MAX_PERIOD,
        .disable_timeout    = DISABLE_TIMEOUT,
        .binding            = binding_mode,
    };

    anjay_iid_t server_instance_id = ANJAY_IID_INVALID;
    if (anjay_server_object_add_instance(anjay, &server_instance,
            &server_instance_id)) {
        log_error(toyota_client, "Could not add server instance");
        goto error;
    }

    // setup custom objects
    humidity_sensor_init_object(anjay);
    headlights_control_init_object(anjay);


    // setup client
    client = (client_t *) avs_calloc(1, sizeof(client_t));
    if (!client) {
        log_error(toyota_client, "Could not allocate client instance");
        goto error;
    }
    client->anjay = anjay;

    // install firmware update object
    if (firmware_update_install(anjay, &client->firmware_update,
                                fw_updated_marker_path, NULL, NULL,
                                fw_update_args)) {
        log_error(toyota_client, "Could not install firmware update object");
        goto error;
    }

    return client;

error:
    if(anjay) anjay_delete(anjay);
    if(client) avs_free(client);
    if(client) firmware_update_destroy(&client->firmware_update);
    return NULL;
}

void
client_destroy(client_t *client_self) {
    if (!client_self) {
        return;
    }

    // release resources
    humidity_sensor_object_release(client_self->anjay);
    headlights_control_object_release(client_self->anjay);
    firmware_update_destroy(&client_self->firmware_update);

    anjay_delete(client_self->anjay);
    avs_free(client_self);
}

void
toyota_client_push_humidity(client_t *self,
                            float sensor_value,
                            bool sensor_state) {
    log_info(toyota_client, "Push HUMIDITY SENSOR object: sensor_value %lf, sensor_state %i",  sensor_value, (int) sensor_state);
    humidity_sensor_set_data(self->anjay, sensor_value, sensor_state);
}

void
toyota_client_push_headlights_control(client_t *self,
                                      bool     control_state,
                                      int64_t  brightness) {
    log_info(toyota_client, "Push HEADLIGHTS CONTROL object: control state %i, brightness %li", (int) control_state, brightness);
    headlights_control_set_data(self->anjay, control_state, brightness);
}

