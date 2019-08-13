#include "humidity.h"
#include "sys/stat.h"
#include "unistd.h"
#include "assert.h"
#include "string.h"
#include "stdio.h"

#define humidity_sensor_log( level, ...) avs_log(toyota_humidity, level, __VA_ARGS__)

typedef struct humidity_instance{
    anjay_iid_t iid;
    char reserved[10];
    float sensor_value;
    bool sensor_state;
    char time[30];
}humidity_instance_t;

typedef struct{
    const anjay_dm_object_def_t *obj_def;
    humidity_instance_t humidity;
}humidity_object_t;

//------------------------------------------------------------------------------

static humidity_object_t *this = NULL;

//------------------------------------------------------------------------------

static
int humidity_resource_read(anjay_t *anjay,
                           const anjay_dm_object_def_t *const *obj_ptr,
                           anjay_iid_t iid,
                           anjay_rid_t rid,
                           anjay_output_ctx_t *ctx) {
    (void) anjay;
    (void) obj_ptr;
    (void) iid;

    humidity_sensor_log(DEBUG, "Read /%i/%i/%i", HUMIDITY_SENSOR_OBJECT_ID, iid, rid);
    humidity_instance_t *inst = &this->humidity;
    assert(inst);

    switch (rid) {
    case HUMIDITY_SENSOR_VALUE: {
        humidity_sensor_log(DEBUG, "|| === || Read humidity sensor value || === ||");
        return anjay_ret_float(ctx, inst->sensor_value);
    }
    case HUMIDITY_SENSOR_STATE: {
        humidity_sensor_log(DEBUG, "|| === || Read humidity sensor state  || === ||");
        return anjay_ret_bool(ctx, inst->sensor_state);
    }
    case HUMIDITY_SENSOR_TIME_STAMP: {
        humidity_sensor_log(DEBUG, "|| === || Read time stamp of last change || === ||");
        return anjay_ret_string(ctx, get_current_time());
    }
    default:
    return ANJAY_ERR_NOT_FOUND;
    }
}

//------------------------------------------------------------------------------

static
int humidity_resource_write(anjay_t *anjay,
                            const anjay_dm_object_def_t *const *obj_ptr,
                            anjay_iid_t iid,
                            anjay_rid_t rid,
                            anjay_input_ctx_t *ctx) {
    (void) anjay;
    (void) obj_ptr;
    (void) iid;

    humidity_sensor_log(DEBUG, "Write /%i/%i/%i", HUMIDITY_SENSOR_OBJECT_ID, iid, rid);
    humidity_instance_t *inst = &this->humidity;
    assert(inst);

    switch (rid) {
    case HUMIDITY_SENSOR_VALUE: {
        float temp_value;
        humidity_sensor_log(DEBUG, "|| === || Write humidity sensor value by server || === ||");
        int result = anjay_get_float(ctx, &temp_value);
        if (result) {
            return result;
        }
        if (temp_value < 0 || temp_value > 40) {
            return ANJAY_ERR_BAD_REQUEST;
        }
        inst->sensor_value = temp_value;
        return 0;
    }
    case HUMIDITY_SENSOR_STATE: {
        bool temp_state;
        humidity_sensor_log(DEBUG, "|| === || Write humidity sensor state by server || === ||");
        bool result = anjay_get_bool(ctx, &temp_state);
        if (result) {
            return result;
        }
        if (temp_state < 0 || temp_state > 1) {
            return ANJAY_ERR_BAD_REQUEST;
        }
        inst->sensor_state = temp_state;
        return 0;
    }
    default:
    return ANJAY_ERR_NOT_FOUND;
    }
}

//------------------------------------------------------------------------------

static
const anjay_dm_object_def_t HUMIDITY_SENSOR_OBJECT_DEFINE = {
    .oid = HUMIDITY_SENSOR_OBJECT_ID,

    .supported_rids = ANJAY_DM_SUPPORTED_RIDS(HUMIDITY_SENSOR_VALUE,
                                              HUMIDITY_SENSOR_STATE,
                                              HUMIDITY_SENSOR_TIME_STAMP),

    .handlers = {

        .instance_it            = anjay_dm_instance_it_SINGLE,
        .instance_present       = anjay_dm_instance_present_SINGLE,

        .resource_present       = anjay_dm_resource_present_TRUE,
        .resource_read          = humidity_resource_read,
        .resource_write         = humidity_resource_write,

        .transaction_begin      = anjay_dm_transaction_NOOP,
        .transaction_validate   = anjay_dm_transaction_NOOP,
        .transaction_commit     = anjay_dm_transaction_NOOP,
        .transaction_rollback   = anjay_dm_transaction_NOOP
    }
};

//------------------------------------------------------------------------------

void
humidity_sensor_init_object(anjay_t * anjay) {
    assert(anjay);

    this=
    (humidity_object_t*)avs_calloc(1, sizeof(humidity_object_t));
    if (!this) {
        humidity_sensor_log(ERROR, "Out of memory");
        return;
    }

    // initialize
    this->obj_def = &HUMIDITY_SENSOR_OBJECT_DEFINE;

    // default (most comfortable) hudimity
    this->humidity.sensor_value = 35.0;
    // hudimity control relay is OFF
    this->humidity.sensor_state = false;
    // return current date and time as string
    memset(this->humidity.time, 0x00, sizeof(this->humidity.time));
    sprintf(this->humidity.time, "%s", get_current_time());

    // register
    if (anjay_register_object(anjay, &this->obj_def)) {
        humidity_sensor_log(ERROR, "Failed to register humidity object");
        avs_free(this);
        this = NULL;
        return;
    }
}

//------------------------------------------------------------------------------

void
humidity_sensor_set_data(anjay_t * anjay,
                         float sensor_value,
                         bool sensor_state) {
    assert(anjay);

    this->humidity.sensor_value = sensor_value;
    this->humidity.sensor_state = sensor_state;
    sprintf(this->humidity.time, "%s", get_current_time());

    anjay_notify_changed(anjay,
                         HUMIDITY_SENSOR_OBJECT_ID,
                         0,
                         HUMIDITY_SENSOR_VALUE);

    anjay_notify_changed(anjay,
                         HUMIDITY_SENSOR_OBJECT_ID,
                         0,
                         HUMIDITY_SENSOR_STATE);

    anjay_notify_changed(anjay,
                         HUMIDITY_SENSOR_OBJECT_ID,
                         0,
                         HUMIDITY_SENSOR_TIME_STAMP);
}

//------------------------------------------------------------------------------

void
humidity_sensor_object_release(anjay_t *anjay){
    assert(anjay);

    if (!this) {
        return;
    }

    anjay_unregister_object(anjay,&this->obj_def);
    avs_free(this);
    this = NULL;
}