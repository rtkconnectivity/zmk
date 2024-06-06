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
#include <zmk/ble.h>
#include <drivers/ext_power.h>
#include "trace.h"
#if IS_ENABLED(CONFIG_ZMK_PPT)
#include <zmk/ppt/keyboard_ppt_app.h>
#endif

#if CONFIG_PM
#include "power_manager_unit_platform.h"
enum PMCheckResult app_enter_dlps_check(void) {
    DBG_DIRECT("app check dlps flag %d", app_global_data.is_app_enabled_dlps);
    return app_global_data.is_app_enabled_dlps ? PM_CHECK_PASS : PM_CHECK_FAIL;
}
static void app_dlps_check_cb_register(void) {
    platform_pm_register_callback_func((void *)app_enter_dlps_check, PLATFORM_PM_CHECK);
}
#endif

int main(void) {
    LOG_INF("Welcome to ZMK!\n");

#if CONFIG_PM
    app_dlps_check_cb_register();
#endif

    if (zmk_kscan_init(DEVICE_DT_GET(ZMK_MATRIX_NODE_ID)) != 0) {
        return -ENOTSUP;
    }
    DBG_DIRECT("app global mode usb %d ppt%d ble%d",app_mode.is_in_usb_mode, app_mode.is_in_ppt_mode,app_mode.is_in_bt_mode);
    if(app_mode.is_in_bt_mode && !app_mode.is_in_usb_mode)
    {
        zmk_ble_init();
    }
#if IS_ENABLED(CONFIG_ZMK_PPT)
    if(app_mode.is_in_ppt_mode && !app_mode.is_in_usb_mode)
    {
        zmk_ppt_init();
    }
#endif

#ifdef CONFIG_ZMK_DISPLAY
    zmk_display_init();
#endif /* CONFIG_ZMK_DISPLAY */

    return 0;
}