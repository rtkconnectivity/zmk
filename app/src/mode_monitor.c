/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <zmk/mode_monitor.h>
#include <zmk/ble.h>
#include "trace.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define PIN_BLE_FLAGS DT_GPIO_FLAGS(DT_NODELABEL(mode_monitor), ble_gpios)
#define PIN_PPT_FLAGS DT_GPIO_FLAGS(DT_NODELABEL(mode_monitor), ppt_gpios)

static struct gpio_dt_spec ppt_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(mode_monitor), ppt_gpios);
static struct gpio_dt_spec bt_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(mode_monitor), ble_gpios);

static struct gpio_dt_spec leds_pwron = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), pwr_gpios);
static struct gpio_dt_spec cap_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), cap_gpios);
static struct gpio_dt_spec num_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), num_gpios);
static struct gpio_dt_spec ppt_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), ppt_gpios);
static struct gpio_dt_spec bt_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), ble_gpios);

static struct gpio_callback zmk_mode_monitor_ppt_cb;
static struct gpio_callback zmk_mode_monitor_bt_cb;

typedef struct APP_MODE {
    bool is_in_bt_mode;
    bool is_in_usb_mode;
    bool is_in_ppt_mode;
} T_APP_MODE;

struct zmk_mode_monitor_msg_processor {
    struct k_work work;
} mm_msg_processor;

struct zmk_mode_monitor_event {
    uint8_t app_cur_mode;
    uint8_t state_changed;
};
static T_APP_MODE app_mode = {false};

K_MSGQ_DEFINE(zmk_mode_monitor_msgq, sizeof(struct zmk_mode_monitor_event), 4, 4);

static void zmk_mode_monitor_callback(const struct device *dev, struct gpio_callback *gpio_cb,
                                      uint32_t pins) {
    DBG_DIRECT("zmk_mode_monitor_callback,pin is %d(256-(2.4G)P1_0, 512(BLE)-P1_1)", pins);
    if (pins == 256) {
        if (app_mode.is_in_ppt_mode == false) {
            gpio_pin_interrupt_configure_dt(&ppt_irq, GPIO_INT_LEVEL_HIGH);
            gpio_pin_set((&ppt_led)->port, (&ppt_led)->pin, 0);
            DBG_DIRECT("[zmk_mode_monitor_callback]:enter ppt mode");
            app_mode.is_in_ppt_mode = true;
            struct zmk_mode_monitor_event ev = {
                .app_cur_mode = PPT_MODE,
                .state_changed = 1
            };
            k_msgq_put(&zmk_mode_monitor_msgq, &ev, K_NO_WAIT);
            k_work_submit(&mm_msg_processor.work);
        } else if (app_mode.is_in_ppt_mode == true) {
            gpio_pin_interrupt_configure_dt(&ppt_irq, GPIO_INT_LEVEL_LOW);
            gpio_pin_set((&ppt_led)->port, (&ppt_led)->pin, 1);
            DBG_DIRECT("[zmk_mode_monitor_callback]:exit ppt mode");
            app_mode.is_in_ppt_mode = false;
            struct zmk_mode_monitor_event ev = {
                .app_cur_mode = PPT_MODE,
                .state_changed = 0
            };
            k_msgq_put(&zmk_mode_monitor_msgq, &ev, K_NO_WAIT);
            k_work_submit(&mm_msg_processor.work);
        }
    } else if (pins == 512) {
        if (app_mode.is_in_bt_mode == false) {
            gpio_pin_interrupt_configure_dt(&bt_irq, GPIO_INT_LEVEL_HIGH);
            gpio_pin_set((&bt_led)->port, (&bt_led)->pin, 0);
            DBG_DIRECT("[zmk_mode_monitor_callback]:enter bt mode");
            app_mode.is_in_bt_mode = true;
            struct zmk_mode_monitor_event ev = {
                .app_cur_mode = BT_MODE,
                .state_changed = 1
            };
            k_msgq_put(&zmk_mode_monitor_msgq, &ev, K_NO_WAIT);
            k_work_submit(&mm_msg_processor.work);
        } else if (app_mode.is_in_bt_mode == true) {
            gpio_pin_interrupt_configure_dt(&bt_irq, GPIO_INT_LEVEL_LOW);
            gpio_pin_set((&bt_led)->port, (&bt_led)->pin, 1);
            DBG_DIRECT("[zmk_mode_monitor_callback]:exit bt mode");
            app_mode.is_in_bt_mode = false;
            struct zmk_mode_monitor_event ev = {
                .app_cur_mode = BT_MODE,
                .state_changed = 0
            };
            k_msgq_put(&zmk_mode_monitor_msgq, &ev, K_NO_WAIT);
            k_work_submit(&mm_msg_processor.work);
        }
    }
}
static void zmk_mode_monitor_handler(struct k_work *item) {
    struct zmk_mode_monitor_event ev;

    while(k_msgq_get(&zmk_mode_monitor_msgq, &ev, K_NO_WAIT) == 0)
    {
        if(ev.app_cur_mode == BT_MODE)
        {
            if(ev.state_changed == 1)
            {
                zmk_ble_init();
            }
            else 
            {
                zmk_ble_deinit();
            }
        }
        else
        {
            return;
        }
    }
    return; 
}

static int zmk_mode_monitor_init(void) {

    DBG_DIRECT("zmk_mode_monitor_init");

    k_work_init(&mm_msg_processor.work,zmk_mode_monitor_handler);

    if (!(gpio_is_ready_dt(&ppt_irq) || gpio_is_ready_dt(&bt_irq))) {
        DBG_DIRECT("ppt or ble leds device \"%s\" is not ready");
        return -ENODEV;
    }
    /* 2.4G Mode monitor pin config */
    gpio_pin_configure_dt(&ppt_irq, GPIO_INPUT | GPIO_PULL_UP | PIN_PPT_FLAGS);
    int rc = gpio_pin_interrupt_configure_dt(&ppt_irq, GPIO_INT_EDGE_TO_INACTIVE);
    if (rc != 0) {
        LOG_ERR("configure zmk ppt leds fail, err:%d ", rc);
    }
    gpio_init_callback(&zmk_mode_monitor_ppt_cb, zmk_mode_monitor_callback, BIT((&ppt_irq)->pin));

    rc = gpio_add_callback((&ppt_irq)->port, &zmk_mode_monitor_ppt_cb);
    if (rc != 0) {
        LOG_ERR("configure zmk ppt leds fail, err:%d ", rc);
    }
    /* ble Mode monitor pin config */
    gpio_pin_configure_dt(&bt_irq, GPIO_INPUT | GPIO_PULL_UP | PIN_BLE_FLAGS);
    rc = gpio_pin_interrupt_configure_dt(&bt_irq, GPIO_INT_EDGE_TO_INACTIVE);
    if (rc != 0) {
        LOG_ERR("configure zmk ppt leds fail, err:%d ", rc);
    }
    gpio_init_callback(&zmk_mode_monitor_bt_cb, zmk_mode_monitor_callback, BIT((&bt_irq)->pin));
    rc = gpio_add_callback((&bt_irq)->port, &zmk_mode_monitor_bt_cb);
    if (rc != 0) {
        LOG_ERR("configure zmk ble leds fail, err:%d ", rc);
    }

    gpio_pin_interrupt_configure_dt(&leds_pwron, GPIO_INT_DISABLE);
    gpio_pin_configure_dt(&leds_pwron, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&cap_led, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&num_led, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&bt_led, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&ppt_led, GPIO_OUTPUT_HIGH);

    return 0;
}

void cap_led_on(void)
{
    gpio_pin_set((&cap_led)->port, (&cap_led)->pin, 0);
}

void cap_led_off(void)
{
    gpio_pin_set((&cap_led)->port, (&cap_led)->pin, 1);
}

void num_led_on(void)
{
    gpio_pin_set((&num_led)->port, (&num_led)->pin, 0);
}

void num_led_off(void)
{
    gpio_pin_set((&num_led)->port, (&num_led)->pin, 1);
}

SYS_INIT(zmk_mode_monitor_init, APPLICATION, 50);