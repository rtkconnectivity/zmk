/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_key_press

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#if IS_ENABLED(CONFIG_SOC_SERIES_RTL87X2G)
#include <zmk/ppt/keyboard_ppt_app.h>
#endif
#include <zmk/behavior.h>
#include <zmk/board.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static int behavior_key_press_init(const struct device *dev) { return 0; };

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);
#if IS_ENABLED(CONFIG_ZMK_PPT)
    if (zmk_ppt_is_ready()) {
#if FEATURE_SUPPORT_2_4G_FAST_KEYSTROKE_PROCESS
		struct zmk_keycode_state_changed ev =
            zmk_keycode_state_changed_from_encoded(binding->param1, true, event.timestamp);
        hid_listener_keycode_pressed(&ev);
#endif
    }
#endif
    return raise_zmk_keycode_state_changed_from_encoded(binding->param1, true, event.timestamp);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);
#if IS_ENABLED(CONFIG_ZMK_PPT)
    if (zmk_ppt_is_ready()) {
#if FEATURE_SUPPORT_2_4G_FAST_KEYSTROKE_PROCESS
	    struct zmk_keycode_state_changed ev =
		    zmk_keycode_state_changed_from_encoded(binding->param1, true, event.timestamp);
	    hid_listener_keycode_released(&ev);
#endif
    }
#endif
    return raise_zmk_keycode_state_changed_from_encoded(binding->param1, false, event.timestamp);
}

static const struct behavior_driver_api behavior_key_press_driver_api = {
    .binding_pressed = on_keymap_binding_pressed, .binding_released = on_keymap_binding_released};

#define KP_INST(n)                                                                                 \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_key_press_init, NULL, NULL, NULL, POST_KERNEL,             \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_key_press_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)
