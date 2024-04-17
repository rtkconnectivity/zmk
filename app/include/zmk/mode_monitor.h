/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/sys/util.h>
#include <zephyr/types.h>

#pragma once

enum app_mode_type {
    BT_MODE = 0,
    PPT_MODE,
    USB_MODE,
};

void cap_led_on(void);
void cap_led_off(void);
void num_led_on(void);
void num_led_off(void);
