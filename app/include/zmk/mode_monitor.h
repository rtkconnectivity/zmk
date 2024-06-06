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

typedef struct APP_MODE {
    bool is_in_bt_mode;
    bool is_in_usb_mode;
    bool is_in_ppt_mode;
} T_APP_MODE;

typedef struct APP_GLOBAL_DATA {
    bool is_app_enabled_dlps;
} T_APP_GLOBAL_DATA;

extern T_APP_MODE app_mode;
extern T_APP_GLOBAL_DATA app_global_data;

enum app_mode_type {
    BT_MODE = 0,
    PPT_MODE,
    USB_MODE,
};

void cap_led_on(void);
void cap_led_off(void);
void num_led_on(void);
void num_led_off(void);

void pm_check_status_before_enter_wfi_or_dlps(void);
void pm_no_check_status_before_enter_wfi(void);