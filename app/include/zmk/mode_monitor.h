/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/sys/util.h>
#include <zephyr/types.h>

#pragma once

#define GPIO_PIN_LEVEL_LOW   0
#define GPIO_PIN_LEVEL_HIGH 1
#define USB_IN_DETECT_NUM 5
#define USB_OUT_DETECT_NUM 6

enum app_mode_type {
    BT_MODE = 0,
    PPT_MODE,
    USB_MODE,
};

void cap_led_on(void);
void cap_led_off(void);
void num_led_on(void);
void num_led_off(void);
