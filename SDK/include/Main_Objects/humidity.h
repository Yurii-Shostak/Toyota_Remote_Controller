#ifndef HUMIDITY_H
#define HUMIDITY_H

#include <anjay/anjay.h>
#include <avsystem/commons/log.h>
#include <avsystem/commons/time.h>
#include <avsystem/commons/memory.h>
#include <avsystem/commons/vector.h>

#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "../toyota_utils.h"

#define HUMIDITY_SENSOR_OBJECT_ID  33204  // humidity sensor object id

#define HUMIDITY_SENSOR_VALUE      5500   // last or current measured value fron the sensor
#define HUMIDITY_SENSOR_STATE      5501   // remote control state of the sensor (ON/OFF)
#define HUMIDITY_SENSOR_TIME_STAMP 5502   // time of last change of humidity sensor value


void 
humidity_sensor_init_object(anjay_t *anjay);

void
humidity_sensor_set_data(anjay_t *anjay,
                         float humidity_value,
                         bool sensor_state);

void
humidity_sensor_object_release(anjay_t *anjay);



#endif // HUMIDITY_H
