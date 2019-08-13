#include "headlights_control.h"
#include "sys/stat.h"
#include "unistd.h"
#include "assert.h"
#include "string.h"
#include "stdio.h"

#define headlights_control_log( level, ...) avs_log(toyota_headlights_control, level, __VA_ARGS__)

typedef struct headlights_instance{
    anjay_iid_t iid;
    char reserved[10];
    bool control_state;
    int64_t brightness;
    char time[30];
}headlights_instance_t;

typedef struct{
    const anjay_dm_object_def_t *obj_def;
    headlights_instance_t headlights;
}headlights_object_t;

//------------------------------------------------------------------------------

static headlights_object_t *this = NULL;

//------------------------------------------------------------------------------

static
int headlights_control_resource_read(anjay_t *anjay,
                                     const anjay_dm_object_def_t *const *obj_ptr,
                                     anjay_iid_t iid,
                                     anjay_rid_t rid,
                                     anjay_output_ctx_t *ctx) {
    (void) anjay;
    (void) obj_ptr;
    (void) iid;

    headlights_control_log(DEBUG, "Read /%i/%i/%i", HEADLIGHTS_CONTROL_OBJECT_ID, iid, rid);
    headlights_instance_t *inst = &this->headlights;
    assert(inst);

    switch (rid) {
    case HEADLIGHTS_CONTROL_STATE: {
        headlights_control_log(DEBUG, "|| === || Read headlights control state value || === ||");
        return anjay_ret_bool(ctx, inst->control_state);
    }
    case HEADLIGHTS_CONTROL_BRIGHTNESS: {
        headlights_control_log(DEBUG, "|| === || Read headlights control brightness value || === ||");
        return anjay_ret_i64(ctx, inst->brightness);
    }
    case HEADLIGHTS_CONTROL_TIME_STAMP: {
        headlights_control_log(DEBUG, "|| === || Read headlights control time stamp value || === ||");
        return anjay_ret_string(ctx, get_current_time());
    }
    default:
    return ANJAY_ERR_NOT_FOUND;
    }
}

//------------------------------------------------------------------------------

static
int headlights_control_resource_write(anjay_t *anjay,
                                      const anjay_dm_object_def_t *const *obj_ptr,
                                      anjay_iid_t iid,
                                      anjay_rid_t rid,
                                      anjay_input_ctx_t *ctx) {
    (void) anjay;
    (void) obj_ptr;
    (void) iid;

    headlights_control_log(DEBUG, "Write /%i/%i/%i", HEADLIGHTS_CONTROL_OBJECT_ID, iid, rid);
    headlights_instance_t *inst = &this->headlights;
    assert(inst);

    switch (rid) {
    case HEADLIGHTS_CONTROL_STATE: {
        bool temp_state;
        headlights_control_log(DEBUG, "|| === || Write headlights control state by server|| === ||");
        bool result = anjay_get_bool(ctx, &temp_state);
        if (result) {
            return result;
        }
        if (temp_state < 0 || temp_state > 1) {
            return ANJAY_ERR_BAD_REQUEST;
        }
        inst->control_state = temp_state;
        return 0;
    }
    case HEADLIGHTS_CONTROL_BRIGHTNESS: {
        int64_t temp_value;
        headlights_control_log(DEBUG, "|| === || Write headlights control brightness value by server || === ||");
        int result = anjay_get_i64(ctx, &temp_value);
        if (result) {
            return result;
        }
        if (temp_value < 0 || temp_value > 100) {
            return ANJAY_ERR_BAD_REQUEST;
        }
        inst->brightness = temp_value;
        return 0;
    }
    default:
    return ANJAY_ERR_NOT_FOUND;
    }
}

//------------------------------------------------------------------------------

static 
const anjay_dm_object_def_t HEADLIGHTS_CONTROL_OBJECT_DEFINE = {
    .oid = HEADLIGHTS_CONTROL_OBJECT_ID,

    .supported_rids = ANJAY_DM_SUPPORTED_RIDS(HEADLIGHTS_CONTROL_STATE,
                                              HEADLIGHTS_CONTROL_BRIGHTNESS,
                                              HEADLIGHTS_CONTROL_TIME_STAMP),

    .handlers = {

        .instance_it            = anjay_dm_instance_it_SINGLE,
        .instance_present       = anjay_dm_instance_present_SINGLE,

        .resource_present       = anjay_dm_resource_present_TRUE,
        .resource_read          = headlights_control_resource_read,
        .resource_write         = headlights_control_resource_write,

        .transaction_begin      = anjay_dm_transaction_NOOP,
        .transaction_validate   = anjay_dm_transaction_NOOP,
        .transaction_commit     = anjay_dm_transaction_NOOP,
        .transaction_rollback   = anjay_dm_transaction_NOOP
    }
};

//------------------------------------------------------------------------------

void
headlights_control_init_object(anjay_t * anjay) {
    assert(anjay);

    this=
    (headlights_object_t*)avs_calloc(1, sizeof(headlights_object_t));
    if (!this) {
        headlights_control_log(ERROR, "Out of memory");
        return;
    }

    // initialize
    this->obj_def = &HEADLIGHTS_CONTROL_OBJECT_DEFINE;

    // headlights control relay is OFF
    this->headlights.control_state = false;
    // default brightness of the headlights
    this->headlights.brightness = 50;
    // return current date and time as string 
    memset(this->headlights.time, 0x00, sizeof(this->headlights.time));
    sprintf(this->headlights.time, "%s", get_current_time());

    // register
    if (anjay_register_object(anjay, &this->obj_def)) {
        headlights_control_log(ERROR, "Failed to register humidity object");
        avs_free(this);
        this = NULL;
        return;
    }
}

//------------------------------------------------------------------------------

void
headlights_control_set_data(anjay_t * anjay,
                                 bool control_state,
                                 int64_t brightness) {
    assert(anjay);

    this->headlights.control_state = control_state;
    this->headlights.brightness = brightness;
    sprintf(this->headlights.time, "%s", get_current_time());

    anjay_notify_changed(anjay,
                         HEADLIGHTS_CONTROL_OBJECT_ID,
                         0,
                         HEADLIGHTS_CONTROL_STATE);
    anjay_notify_changed(anjay,
                         HEADLIGHTS_CONTROL_OBJECT_ID,
                         0,
                         HEADLIGHTS_CONTROL_BRIGHTNESS);
    anjay_notify_changed(anjay,
                         HEADLIGHTS_CONTROL_OBJECT_ID,
                         0,
                         HEADLIGHTS_CONTROL_TIME_STAMP);
}

//------------------------------------------------------------------------------

void
headlights_control_object_release(anjay_t *anjay){
    assert(anjay);

    if (!this) {
        return;
    }

    anjay_unregister_object(anjay,&this->obj_def);
    avs_free(this);
    this = NULL;
}