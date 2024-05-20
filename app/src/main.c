/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/settings/settings.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/matrix.h>
#include <zmk/kscan.h>
#include <zmk/display.h>
#include <zmk/mode_monitor.h>
#include <zmk/ppt/keyboard_ppt_app.h>
#include <zmk/ble.h>
#include <drivers/ext_power.h>
#include "trace.h"

int main(void) {
    LOG_INF("Welcome to ZMK!\n");

    if (zmk_kscan_init(DEVICE_DT_GET(ZMK_MATRIX_NODE_ID)) != 0) {
        return -ENOTSUP;
    }
    DBG_DIRECT("app global mode usb %d ppt%d ble%d",app_mode.is_in_usb_mode, app_mode.is_in_ppt_mode,app_mode.is_in_bt_mode);
    if(app_mode.is_in_bt_mode && !app_mode.is_in_usb_mode)
    {
        zmk_ble_init();
    }
    if(app_mode.is_in_ppt_mode && !app_mode.is_in_usb_mode)
    {
        zmk_ppt_init();
    }

#ifdef CONFIG_ZMK_DISPLAY
    zmk_display_init();
#endif /* CONFIG_ZMK_DISPLAY */

    return 0;
}
