#define DT_DRV_COMPAT zmk_behavior_auto_hold

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_auto_hold_config {
    struct zmk_behavior_binding binding;
    int timeout_ms;
};

struct behavior_auto_hold_data {
    int32_t position;
    bool is_auto_held;
    int64_t press_timestamp;
    struct k_work_delayable timeout_work;
    struct zmk_behavior_binding binding;
    uint8_t held_layer;

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    struct behavior_parameter_metadata_set set;
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

static const struct device* devices[CONFIG_ZMK_BEHAVIOR_AUTO_HOLD_MAX_BEHAVIORS];
static uint8_t behavior_auto_hold_count;

static void auto_hold_timeout_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct behavior_auto_hold_data *data =
        CONTAINER_OF(dwork, struct behavior_auto_hold_data, timeout_work);

    data->is_auto_held = true;
    LOG_DBG("Auto-hold activated at position %d", data->position);
}

static int on_auto_hold_binding_pressed(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_auto_hold_config *cfg = dev->config;
    struct behavior_auto_hold_data *data = dev->data;

    if (data->is_auto_held) {
        data->is_auto_held = false;
        k_work_cancel_delayable(&data->timeout_work);

        const struct zmk_behavior_binding new_binding = {
            .behavior_dev = cfg->binding.behavior_dev,
            .param1 = binding->param1,
            .param2 = binding->param2,
        };
        return zmk_behavior_invoke_binding(&new_binding, event, false);
    }

    data->position = event.position;
    data->is_auto_held = false;
    data->press_timestamp = event.timestamp;

    LOG_DBG("Waiting for timeout to start auto hold");

    k_work_schedule(&data->timeout_work, K_MSEC(cfg->timeout_ms));

    const struct zmk_behavior_binding new_binding = {
        .behavior_dev = cfg->binding.behavior_dev,
        .param1 = binding->param1,
        .param2 = binding->param2,
    };

    data->binding = new_binding;
    data->held_layer = event.layer;
    return zmk_behavior_invoke_binding(&new_binding, event, true);
}

static int on_auto_hold_binding_released(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_auto_hold_config *cfg = dev->config;
    struct behavior_auto_hold_data *data = dev->data;

    k_work_cancel_delayable(&data->timeout_work);
    if (data->is_auto_held) {
        LOG_DBG("Auto-hold active, not releasing");
        return ZMK_BEHAVIOR_OPAQUE;
    }

    const struct zmk_behavior_binding new_binding = {
        .behavior_dev = cfg->binding.behavior_dev,
        .param1 = binding->param1,
        .param2 = binding->param2,
    };

    return zmk_behavior_invoke_binding(&new_binding, event, false);
}

static int auto_hold_keycode_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    for (int i = 0; i < behavior_auto_hold_count; i++) {
        const struct device *dev = devices[i];
        if (dev == NULL) {
            continue;
        }

        struct behavior_auto_hold_data *data = dev->data;
        if (data->is_auto_held) {
            LOG_DBG("Releasing auto-held key at position %d", data->position);
            const struct zmk_behavior_binding_event release_event = {
                .position = data->position,
                .timestamp = k_uptime_get(),
                .layer = data->held_layer,
            };

            k_work_cancel_delayable(&data->timeout_work);
            zmk_behavior_invoke_binding(&data->binding, release_event, false);
            data->is_auto_held = false;
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(behavior_auto_hold, auto_hold_keycode_listener);
ZMK_SUBSCRIPTION(behavior_auto_hold, zmk_keycode_state_changed);

static int behavior_auto_hold_init(const struct device *dev) {
    struct behavior_auto_hold_data *data = dev->data;
    k_work_init_delayable(&data->timeout_work, auto_hold_timeout_handler);

    devices[behavior_auto_hold_count] = dev;
    behavior_auto_hold_count++;

    return 0;
}


#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
static int auto_hold_parameter_metadata(const struct device *dev,
                                       struct behavior_parameter_metadata *param_metadata) {
    const struct behavior_auto_hold_config *cfg = dev->config;
    struct behavior_auto_hold_data *data = dev->data;
    struct behavior_parameter_metadata child_meta;

    const int err = behavior_get_parameter_metadata(zmk_behavior_get_binding(cfg->binding.behavior_dev), &child_meta);
    if (err < 0) {
        LOG_WRN("Failed to get the hold behavior parameter: %d", err);
        return err;
    }

    if (child_meta.sets_len > 0) {
        data->set.param1_values = child_meta.sets[0].param1_values;
        data->set.param1_values_len = child_meta.sets[0].param1_values_len;
    }

    param_metadata->sets = &data->set;
    param_metadata->sets_len = 1;
    return 0;
}

#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_driver_api behavior_auto_hold_driver_api = {
    .binding_pressed = on_auto_hold_binding_pressed,
    .binding_released = on_auto_hold_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = &auto_hold_parameter_metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define AH_INST(n)                                                                          \
    static struct behavior_auto_hold_data behavior_auto_hold_data_##n = {};                 \
    static const struct behavior_auto_hold_config behavior_auto_hold_config_##n = {         \
        .binding = {                                                                        \
            .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, 0)),         \
            .param1 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, 0, param1),      \
                                  (0), (DT_INST_PHA_BY_IDX(n, bindings, 0, param1))),       \
        },                                                                                  \
        .timeout_ms = DT_INST_PROP_OR(n, timeout_ms,                                        \
                                      CONFIG_ZMK_BEHAVIOR_AUTO_HOLD_TIMEOUT_MS),            \
    };                                                                                      \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_auto_hold_init, NULL, &behavior_auto_hold_data_##n, \
                           &behavior_auto_hold_config_##n, POST_KERNEL,                     \
                           CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                             \
                           &behavior_auto_hold_driver_api);

DT_INST_FOREACH_STATUS_OKAY(AH_INST)
