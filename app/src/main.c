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
#include <zmk/board.h>
#include <zmk/app_wdt.h>
#include <drivers/ext_power.h>
#include <zephyr/sys/reboot.h>
#include "reset_reason.h"
#include "aon_reg.h"
#include "trace.h"
#if IS_ENABLED(CONFIG_ZMK_PPT)
#include <zmk/ppt/keyboard_ppt_app.h>
#endif

T_APP_MODE app_mode = {.is_in_single_test_mode = false};
T_APP_GLOBAL_DATA app_global_data = {.is_app_enabled_dlps = true};

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
    DBG_DIRECT("Welcome to ZMK!\n");
#if CONFIG_PM
    app_dlps_check_cb_register();
#endif
	// AON_NS_REG0X_APP_TYPE aon_0x1ae0 = {.d32 = AON_REG_READ(AON_NS_REG0X_APP)};
	// uint32_t sw_reset_type = aon_0x1ae0.reset_reason;
    // if (sw_reset_type == SWITCH_TO_TEST_MODE) {
    //     app_mode.is_in_single_test_mode = true;
// #if (MP_TEST_SINGLE_TONE_MODE == GAP_LAYER_SINGLE_TONE_INTERFACE)
//         LOG_INF("GAP_SINGLE_TONE_MODE");
//         // not currently supported
//         if (app_global_data.is_watchdog_enable) {
//             app_watchdog_close();  /* Avoid unexpected reboot */
//         }
// #elif MP_TEST_SINGLE_TONE_MODE == HCI_LAYER_SINGLE_TONE_INTERFACE
//         LOG_INF("HCI_SINGLE_TONE_MODE");
//         if (app_global_data.is_watchdog_enable) {
//             app_watchdog_close();  /* Avoid unexpected reboot */
//         }
//         single_tone_init();
// #endif // MP_TEST_SINGLE_TONE_MODE
    // } else {
        if (zmk_kscan_init(DEVICE_DT_GET(ZMK_MATRIX_NODE_ID)) != 0) {
            return -ENOTSUP;
        }

        zmk_ble_init();

// #if (THE_WAY_TO_ENTER_MP_TEST_MODE == ENTER_MP_TEST_MODE_BY_GPIO_TRIGGER)
//         bool mode_type = mp_test_mode_check_and_enter();
//         if (mode_type) {
//             LOG_DBG("reboot to switch to test mode");
//             sys_reboot(SWITCH_TO_TEST_MODE);
//         }
// #endif

// #ifdef CONFIG_ZMK_DISPLAY
//         zmk_display_init();
// #endif /* CONFIG_ZMK_DISPLAY */
    // }
    return 0;
}