/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#define GPIO_PIN_LEVEL_LOW 0
#define GPIO_PIN_LEVEL_HIGH 1

/**
 * MP test config
 */
#define FEATURE_SUPPORT_MP_TEST_MODE 1  /* set 1 to support mp test mode */

#if FEATURE_SUPPORT_MP_TEST_MODE
#define GAP_LAYER_SINGLE_TONE_INTERFACE 0   /* use gap layer api to send single tone */
#define HCI_LAYER_SINGLE_TONE_INTERFACE 1   /* use HCI api to send single tone */
#define MP_TEST_SINGLE_TONE_MODE HCI_LAYER_SINGLE_TONE_INTERFACE

#define ENTER_MP_TEST_MODE_BY_GPIO_TRIGGER 0
#define ENTER_MP_TEST_MODE_BY_USB_CMD 1
#endif

#if (MP_TEST_SINGLE_TONE_MODE == GAP_LAYER_SINGLE_TONE_INTERFACE)
#define THE_WAY_TO_ENTER_MP_TEST_MODE ENTER_MP_TEST_MODE_BY_USB_CMD
#elif (MP_TEST_SINGLE_TONE_MODE == HCI_LAYER_SINGLE_TONE_INTERFACE)
#define THE_WAY_TO_ENTER_MP_TEST_MODE ENTER_MP_TEST_MODE_BY_GPIO_TRIGGER
#endif

typedef struct APP_MODE {
    bool is_in_single_test_mode;
} T_APP_MODE;

typedef struct APP_GLOBAL_DATA {
    bool is_app_enabled_dlps;
    bool is_watchdog_enable;
} T_APP_GLOBAL_DATA;

extern T_APP_GLOBAL_DATA app_global_data;