/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/settings/settings.h>
#include <zmk/app_wdt.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "trace.h"

static void app_wdt_timeout_cb(struct k_timer *timer);
static K_TIMER_DEFINE(app_wdt_timer, app_wdt_timeout_cb, NULL);
const struct device *const wdt = DEVICE_DT_GET(DT_ALIAS(watchdog));


void app_system_reset(uint8_t flag)
{
    struct wdt_timeout_cfg wdt_config = {
        .flags = flag,
        .window.max = 0, //set window.max to 0: reboot immediately
    };
    wdt_install_timeout(wdt,&wdt_config);
}

static int app_wdt_init(void)
{
    if(!device_is_ready(wdt))
    {
        LOG_DBG("%s:device not ready",wdt->name);
        return 0;
    }
    k_timer_start(&app_wdt_timer, K_MSEC(4000),K_MSEC(4000));
}

static void app_wdt_timeout_cb(struct k_timer *timer)
{
    //DBG_DIRECT("watchdog feed");
    wdt_feed(wdt,0);
    return;
}

SYS_INIT(app_wdt_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);