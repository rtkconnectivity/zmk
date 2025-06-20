/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>
#include <zmk/keys.h>
#include <zmk/leds/leds_gpio_driver.h>

struct zmk_keycode_state_changed {
    uint16_t usage_page;
    uint32_t keycode;
    uint8_t implicit_modifiers;
    uint8_t explicit_modifiers;
    bool state;
    int64_t timestamp;
};

ZMK_EVENT_DECLARE(zmk_keycode_state_changed);

static inline struct zmk_keycode_state_changed
zmk_keycode_state_changed_from_encoded(uint32_t encoded, bool pressed, int64_t timestamp) {
    uint16_t page = ZMK_HID_USAGE_PAGE(encoded);
    uint16_t id = ZMK_HID_USAGE_ID(encoded);
    uint8_t implicit_modifiers = 0x00;
    uint8_t explicit_modifiers = 0x00;
    static uint8_t caps_lock_flag = 0;
    static uint8_t num_lock_flag = 0;

    if (!page) {
        page = HID_USAGE_KEY;
    }

    if (is_mod(page, id)) {
        explicit_modifiers = SELECT_MODS(encoded);
    } else {
        implicit_modifiers = SELECT_MODS(encoded);
    }
#if IS_ENABLED(CONFIG_SOC_SERIES_RTL87X2G)
    if (page == HID_USAGE_KEY) {
        if (id == HID_USAGE_KEY_KEYBOARD_CAPS_LOCK && pressed == 1 && caps_lock_flag == 0) {
            LED_ON(LED_CAPSLOCK);
            caps_lock_flag = 1;
        } else if (id == HID_USAGE_KEY_KEYBOARD_CAPS_LOCK && pressed == 1 && caps_lock_flag == 1) {
            LED_OFF(LED_CAPSLOCK);
            caps_lock_flag = 0;
        } else if (id == HID_USAGE_KEY_KEYPAD_NUM_LOCK_AND_CLEAR && pressed == 1 &&
                   num_lock_flag == 0) {
            LED_ON(LED_NUMLOCK);
            num_lock_flag = 1;
        } else if (id == HID_USAGE_KEY_KEYPAD_NUM_LOCK_AND_CLEAR && pressed == 1 &&
                   num_lock_flag == 1) {
            LED_OFF(LED_NUMLOCK);
            num_lock_flag = 0;
        }
    }
#endif
    return (struct zmk_keycode_state_changed){.usage_page = page,
                                              .keycode = id,
                                              .implicit_modifiers = implicit_modifiers,
                                              .explicit_modifiers = explicit_modifiers,
                                              .state = pressed,
                                              .timestamp = timestamp};
}

static inline int raise_zmk_keycode_state_changed_from_encoded(uint32_t encoded, bool pressed,
                                                               int64_t timestamp) {
    return raise_zmk_keycode_state_changed(
        zmk_keycode_state_changed_from_encoded(encoded, pressed, timestamp));
}
