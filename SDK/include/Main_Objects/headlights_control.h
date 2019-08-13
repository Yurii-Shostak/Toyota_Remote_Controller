#ifndef HEADLIGHTS_CONTROL_H
#define HEADLIGHTS_CONTROL_H

#include <anjay/anjay.h>
#include <avsystem/commons/log.h>
#include <avsystem/commons/time.h>
#include <avsystem/commons/memory.h>
#include <avsystem/commons/vector.h>

#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "../toyota_utils.h"

#define HEADLIGHTS_CONTROL_OBJECT_ID  33205 // heghlights control object id

#define HEADLIGHTS_CONTROL_STATE      5503  // state of heghlights control (ON/OFF)
#define HEADLIGHTS_CONTROL_BRIGHTNESS 5504  // heghlights control bright level
#define HEADLIGHTS_CONTROL_TIME_STAMP 5505  // time of last change of control state

void
headlights_control_init_object(anjay_t * anjay);

void
headlights_control_set_data(anjay_t * anjay,
                            bool control_state,
                            int64_t brightness);

void
headlights_control_object_release(anjay_t *anjay);

#endif // HEADLIGHTS_CONTROL_H