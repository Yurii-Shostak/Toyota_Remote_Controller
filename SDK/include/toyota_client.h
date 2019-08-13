#ifndef TOYOTA_CLIENT
#define TOYOTA_CLIENT

#include "toyota_utils.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cpusplus
extern "C" {
#endif

typedef struct client client_t;

/**
 * @brief Create new client
 *
 * Create, configure and start client in separate thread
 *
 * @param ssid                   Short server ID
 * @param endpoint_name          Client name (Device ID)
 * @param server_uri             Server URI
 * @param binding_mode           Bindig mode
 * @param lifetime               Client lifetime
 * @param bootstrap_state        Client bootstrap on/off
 * @param fw_updated_marker_path Client firmware update persistence file
 * @param fw_update_args         Command-line arguments to use for process
 *                               restart after firmware installation
 *
 * @return pointer to the new client instance, NULL in case of error.
 */
client_t *
remote_client_create(uint16_t          ssid,
                     const char        *endpoint_name,
                     const char        *server_uri,
                     const char        *binding_mode,
                     int               lifetime,
                     bool              bootstrap_state,
                     const char        *fw_updated_marker_path,
                     const char *const *fw_update_args);
/**
 * @brief Destroy client instance
 *
 * Stop client and release all allocated resources.
 *
 * @param self Pointer to client object
 */
void
client_destroy(client_t *self);
/**
 * @brief client_poll_sockets
 *
 * Run main client loop once to process pending events.
 *
 * @param self              Pointer to client object
 * @param max_wait_time_ms  Max time to wait for IO events, in milliseconds
 */
void 
remote_client_poll_sockets(client_t *self, int max_wait_time_ms);
/**
 * @brief toyota_client_push_humidity
 *
 * Notify server about new value of humidity object.
 *
 * @param self              Pointer to client object
 * @param sensor_value      Current measured value of humidity
 * @param sensor_state      True - when sensor is ON, false - when sensor is OFF
 */
void
toyota_client_push_humidity(client_t *self,
                            float sensor_value,
                            bool sensor_state);
/**
 * @brief toyota_client_push_headlights_control
 *
 * Notify server about new value of headlights_control object.
 *
 * @param self              Pointer to client object
 * @param control_state     True - when relay is ON, false - when relay is OFF
 * @param brightness        Brightness level of headlights (set by PWM)
 */
void
toyota_client_push_headlights_control(client_t *self,
                                      bool     control_state,
                                      int64_t  brightness);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif //  TOYOTA_CLIENT
