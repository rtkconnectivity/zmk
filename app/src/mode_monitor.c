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
#include <zmk/usb.h>
#include <zmk/app_wdt.h>
#include <zephyr/drivers/watchdog.h>
#include <zmk/ppt/keyboard_ppt_app.h>
#include <zmk/keymap.h>
#if IS_ENABLED(CONFIG_PM)
#include <pm.h>
#endif

#include "trace.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define PIN_BLE_FLAGS DT_GPIO_FLAGS(DT_NODELABEL(mode_monitor), ble_gpios)
#define PIN_PPT_FLAGS DT_GPIO_FLAGS(DT_NODELABEL(mode_monitor), ppt_gpios)
#define PIN_USB_FLAGS DT_GPIO_FLAGS(DT_NODELABEL(mode_monitor), detect_usb)

static struct gpio_dt_spec ppt_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(mode_monitor), ppt_gpios);
static struct gpio_dt_spec bt_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(mode_monitor), ble_gpios);
static struct gpio_dt_spec detect_usb = GPIO_DT_SPEC_GET(DT_NODELABEL(mode_monitor), detect_usb_gpios);
static struct gpio_dt_spec win2mac = GPIO_DT_SPEC_GET(DT_NODELABEL(mode_monitor), win2mac_gpios);

static struct gpio_dt_spec leds_pwron = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), pwr_gpios);
static struct gpio_dt_spec cap_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), cap_gpios);
static struct gpio_dt_spec num_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), num_gpios);
static struct gpio_dt_spec ppt_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), ppt_gpios);
static struct gpio_dt_spec bt_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), ble_gpios);

static struct gpio_callback zmk_mode_monitor_ppt_cb;
static struct gpio_callback zmk_mode_monitor_bt_cb;
static struct gpio_callback zmk_mode_monitor_usb_cb;

static uint8_t usb_in_high_vol_num = 0;
static uint8_t usb_out_low_vol_num = 0;
static uint8_t usb_in_debonce_timer_num = 0;
static uint8_t usb_out_debonce_timer_num = 0;
static bool is_usb_in_debonce_check = false;
static bool is_usb_out_debonce_check = false;
static bool usb_mode_monitor_trigger_level = GPIO_PIN_LEVEL_HIGH;
static void usb_mode_monitor_debounce_timeout_cb(struct k_timer *timer);

static bool is_check_status_before_enter_wfi = true;

static K_TIMER_DEFINE(usb_mode_monitor_timer, usb_mode_monitor_debounce_timeout_cb, NULL);

struct zmk_mode_monitor_msg_processor {
    struct k_work work;
} mm_msg_processor;

struct zmk_mode_monitor_event {
    uint8_t app_cur_mode;
    uint8_t state_changed;
};
T_APP_MODE app_mode = {false};
T_APP_GLOBAL_DATA app_global_data = {.is_app_enabled_dlps = true, .is_usb_enumeration_success = false};

K_MSGQ_DEFINE(zmk_mode_monitor_msgq, sizeof(struct zmk_mode_monitor_event), 4, 4);

static void zmk_mode_monitor_callback(const struct device *dev, struct gpio_callback *gpio_cb,
                                      uint32_t pins) {
    LOG_DBG("zmk_mode_monitor_callback, pins is %u,", pins);

    if ((!strcmp(dev->name,ppt_irq.port->name)) && (pins >> ppt_irq.pin == 1)) {
        if (app_mode.is_in_ppt_mode == false) {
            gpio_pin_interrupt_configure_dt(&ppt_irq, GPIO_INT_LEVEL_HIGH);
            gpio_pin_set(ppt_led.port, ppt_led.pin, 0);
            LOG_DBG("[zmk_mode_monitor_callback]:enter ppt mode");
            app_mode.is_in_ppt_mode = true;
            struct zmk_mode_monitor_event ev = {
                .app_cur_mode = PPT_MODE,
                .state_changed = 1
            };
            k_msgq_put(&zmk_mode_monitor_msgq, &ev, K_NO_WAIT);
            k_work_submit(&mm_msg_processor.work);
        } else if (app_mode.is_in_ppt_mode == true) {
            gpio_pin_interrupt_configure_dt(&ppt_irq, GPIO_INT_LEVEL_LOW);
            gpio_pin_set(ppt_led.port, ppt_led.pin, 1);
            LOG_DBG("[zmk_mode_monitor_callback]:exit ppt mode");
            app_mode.is_in_ppt_mode = false;
            struct zmk_mode_monitor_event ev = {
                .app_cur_mode = PPT_MODE,
                .state_changed = 0
            };
            k_msgq_put(&zmk_mode_monitor_msgq, &ev, K_NO_WAIT);
            k_work_submit(&mm_msg_processor.work);
        }
    } else if ((!strcmp(dev->name,bt_irq.port->name)) && (pins >> bt_irq.pin == 1)) {
        if (app_mode.is_in_bt_mode == false) {
            gpio_pin_interrupt_configure_dt(&bt_irq, GPIO_INT_LEVEL_HIGH);
            gpio_pin_set(bt_led.port, bt_led.pin, 0);
            LOG_DBG("[zmk_mode_monitor_callback]:enter bt mode");
            app_mode.is_in_bt_mode = true;
            struct zmk_mode_monitor_event ev = {
                .app_cur_mode = BT_MODE,
                .state_changed = 1
            };
            k_msgq_put(&zmk_mode_monitor_msgq, &ev, K_NO_WAIT);
            k_work_submit(&mm_msg_processor.work);
        } else if (app_mode.is_in_bt_mode == true) {
            gpio_pin_interrupt_configure_dt(&bt_irq, GPIO_INT_LEVEL_LOW);
            gpio_pin_set(bt_led.port, bt_led.pin, 1);
            LOG_DBG("[zmk_mode_monitor_callback]:exit bt mode");
            app_mode.is_in_bt_mode = false;
            struct zmk_mode_monitor_event ev = {
                .app_cur_mode = BT_MODE,
                .state_changed = 0
            };
            k_msgq_put(&zmk_mode_monitor_msgq, &ev, K_NO_WAIT);
            k_work_submit(&mm_msg_processor.work);
        }
    }
    else if((!strcmp(dev->name,win2mac.port->name)) && (pins >> win2mac.pin == 1))
    {
        LOG_DBG("gpio callback win2macos");
        if(!gpio_pin_get_raw(win2mac.port,win2mac.pin) && app_mode.is_in_macos)
        {
            LOG_DBG("gpio_pin_get_raw win2mac low, set to windows");
            keyboard_switch_os();
            app_mode.is_in_windows = true;
            gpio_pin_interrupt_configure_dt(&win2mac, GPIO_INT_LEVEL_HIGH);
        }
        else if(gpio_pin_get_raw(win2mac.port,win2mac.pin) && app_mode.is_in_windows)
        {
            LOG_DBG("gpio_pin_get_raw win2mac low, set to macos");
            keyboard_switch_os();
            app_mode.is_in_macos = true;
            gpio_pin_interrupt_configure_dt(&win2mac, GPIO_INT_LEVEL_LOW);
        }
    }
    else if((!strcmp(dev->name,detect_usb.port->name)) && (pins >> detect_usb.pin == 1))
    {
        gpio_pin_interrupt_configure_dt(&detect_usb, GPIO_INT_DISABLE);
        
        if(usb_mode_monitor_trigger_level == GPIO_PIN_LEVEL_HIGH)
        {
            is_usb_in_debonce_check = true;
            usb_in_debonce_timer_num = 0;
        }
        else
        {
            gpio_pin_interrupt_configure_dt(&detect_usb, GPIO_INT_DISABLE);
            is_usb_out_debonce_check = true;
            usb_out_debonce_timer_num = 0;
        }
        k_timer_start(&usb_mode_monitor_timer, K_MSEC(50),K_NO_WAIT);
    }
}
/* zmk mode monitor handler */
static void zmk_mode_monitor_handler(struct k_work *item) {
    struct zmk_mode_monitor_event ev;

    while(k_msgq_get(&zmk_mode_monitor_msgq, &ev, K_NO_WAIT) == 0)
    {
        if(ev.app_cur_mode == BT_MODE)
        {
            if(ev.state_changed == 1)
            {
                if(app_mode.is_in_usb_mode && app_global_data.is_usb_enumeration_success)
                {
                    return;
                }
                else
                {
                    LOG_DBG("[zmk_mode_monitor_handler]:reset to enter bt mode");
                    app_system_reset(WDT_FLAG_RESET_SOC);
                }
            }
            else 
            {
                if(app_mode.is_in_usb_mode && app_global_data.is_usb_enumeration_success)
                {
                    return;
                }
                LOG_DBG("[zmk_mode_monitor_handler]: exit bt mode");
                app_system_reset(WDT_FLAG_RESET_SOC);
            }
        }
        else if(ev.app_cur_mode == USB_MODE)
        {
            if(ev.state_changed == 1)
            {
                zmk_usb_init();
            }
            else
            {
                zmk_usb_deinit();
                if(app_mode.is_in_bt_mode || app_mode.is_in_ppt_mode)
                {
                    LOG_DBG("[zmk_mode_monitor_handler]:reset to exit usb and enter bt or ppt mode");
                    app_system_reset(WDT_FLAG_RESET_SOC);
                }
            }
        }
        else if(ev.app_cur_mode == PPT_MODE)
        {
            if(ev.state_changed == 1)
            {
#if IS_ENABLED(CONFIG_ZMK_PPT)
                zmk_ppt_init();
#endif
            }
            else
            {
                return;
            }
        }
    }
    return; 
}

static int zmk_mode_monitor_init(void) {

    LOG_DBG("zmk_mode_monitor_init");

    k_work_init(&mm_msg_processor.work,zmk_mode_monitor_handler);

    /* gpio leds config */
    int err = gpio_pin_interrupt_configure_dt(&leds_pwron, GPIO_INT_DISABLE);
    if(err)
    {
        LOG_ERR("Unable to disable irq of pin %u on %s,err %d",leds_pwron.pin,leds_pwron.port->name,err);
    }
    gpio_pin_configure_dt(&leds_pwron, GPIO_OUTPUT_LOW);
    gpio_pin_configure_dt(&cap_led, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&num_led, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&bt_led, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&ppt_led, GPIO_OUTPUT_HIGH);

    if (!(gpio_is_ready_dt(&ppt_irq) || gpio_is_ready_dt(&bt_irq) || gpio_is_ready_dt(&win2mac))) {
        LOG_ERR("ppt or ble or win2mac device is not ready");
        return -ENODEV;
    }

    /* 2.4G Mode monitor callback enable and pin config */
    gpio_init_callback(&zmk_mode_monitor_ppt_cb, zmk_mode_monitor_callback, BIT((&ppt_irq)->pin));
     int  rc = gpio_add_callback((&ppt_irq)->port, &zmk_mode_monitor_ppt_cb);
    if (rc != 0) {
        LOG_ERR("configure zmk ppt leds fail, err:%d ", rc);
    }

    gpio_pin_configure_dt(&ppt_irq, GPIO_INPUT | GPIO_PULL_UP | PIN_PPT_FLAGS);
    if(!gpio_pin_get_raw(ppt_irq.port,ppt_irq.pin))
    {
        app_mode.is_in_ppt_mode = true;
        gpio_pin_set(ppt_led.port, ppt_led.pin, 0);
        rc = gpio_pin_interrupt_configure_dt(&ppt_irq, GPIO_INT_LEVEL_HIGH);
    }
    else
    {
        rc = gpio_pin_interrupt_configure_dt(&ppt_irq, GPIO_INT_LEVEL_LOW);
    }
    if (rc != 0) {
        LOG_ERR("configure zmk ppt leds fail, err:%d ", rc);
    }

    /* ble Mode monitor callback enable and pin config */
    gpio_init_callback(&zmk_mode_monitor_bt_cb, zmk_mode_monitor_callback, BIT((&bt_irq)->pin));
    rc = gpio_add_callback((&bt_irq)->port, &zmk_mode_monitor_bt_cb);
    if (rc != 0) {
        LOG_ERR("configure zmk ble leds fail, err:%d ", rc);
    }
    gpio_pin_configure_dt(&bt_irq, GPIO_INPUT | GPIO_PULL_UP | PIN_BLE_FLAGS);
    if(!gpio_pin_get_raw(bt_irq.port,bt_irq.pin))
    {
        app_mode.is_in_bt_mode = true;
        gpio_pin_set(bt_led.port, bt_led.pin, 0);
        rc = gpio_pin_interrupt_configure_dt(&bt_irq, GPIO_INT_LEVEL_HIGH);
    }
    else
    {
        rc = gpio_pin_interrupt_configure_dt(&bt_irq, GPIO_INT_LEVEL_LOW);
    }
    if (rc != 0) {
        LOG_ERR("configure zmk ppt leds fail, err:%d ", rc);
    }

     /* win or mac Mode monitor callback enable and pin config */
    gpio_init_callback(&zmk_mode_monitor_bt_cb, zmk_mode_monitor_callback, BIT((&win2mac)->pin));
    rc = gpio_add_callback((&win2mac)->port, &zmk_mode_monitor_bt_cb);
    if (rc != 0) {
        LOG_ERR("configure win2mac gpio cb fail, err:%d ", rc);
    }
    gpio_pin_configure_dt(&win2mac, GPIO_INPUT | GPIO_PULL_UP | PIN_BLE_FLAGS);
    if(!gpio_pin_get_raw(win2mac.port,win2mac.pin))
    {
        LOG_DBG("gpio_pin_get_raw win2mac low, set to windows");
        app_mode.is_in_windows = true;
        rc = gpio_pin_interrupt_configure_dt(&win2mac, GPIO_INT_LEVEL_HIGH);
    }
    else
    {
        LOG_DBG("gpio_pin_get_raw win2mac low, set to macos");
        keyboard_switch_os();
        app_mode.is_in_macos = true;
        rc = gpio_pin_interrupt_configure_dt(&win2mac, GPIO_INT_LEVEL_LOW);
    }
    if (rc != 0) {
        LOG_ERR("configure win2mac gpio interrupt fail, err:%d ", rc);
    }

    /* usb Mode monitor callback enable and pin config */
    gpio_init_callback(&zmk_mode_monitor_usb_cb, zmk_mode_monitor_callback, BIT((&detect_usb)->pin));
    rc = gpio_add_callback((&detect_usb)->port, &zmk_mode_monitor_usb_cb);
    if (rc != 0) {
        LOG_ERR("configure zmk usb cb fail, err:%d ", rc);
    }
    gpio_pin_configure_dt(&detect_usb, GPIO_INPUT);
    rc = gpio_pin_interrupt_configure_dt(&detect_usb, GPIO_INT_LEVEL_HIGH);
    if (rc != 0) {
        LOG_ERR("configure zmk usb leds fail, err:%d ", rc);
    }

    return 0;
}

static void usb_mode_monitor_debounce_timeout_cb(struct k_timer *timer)
{
    int usb_pin_polarity_status = gpio_pin_get_raw(detect_usb.port, detect_usb.pin);

    if(is_usb_in_debonce_check)
    {
        app_global_data.is_app_enabled_dlps = false;
        if(usb_pin_polarity_status == GPIO_PIN_LEVEL_HIGH)
        {
            LOG_DBG("[usb_mode_monitor_debounce_timeout_cb]: usb_in_high_vol is %d",usb_in_high_vol_num);
            usb_in_high_vol_num ++;
            if(usb_in_high_vol_num >= USB_IN_DETECT_NUM)
            {
                usb_out_low_vol_num = 0;
                /* usb in */
                is_usb_in_debonce_check = false;
                gpio_pin_interrupt_configure_dt(&detect_usb, GPIO_INT_LEVEL_LOW);
                usb_mode_monitor_trigger_level = GPIO_PIN_LEVEL_LOW;
 
                struct zmk_mode_monitor_event ev = {
                    .app_cur_mode = USB_MODE,
                    .state_changed = 1
                };
                k_msgq_put(&zmk_mode_monitor_msgq, &ev, K_NO_WAIT);
                k_work_submit(&mm_msg_processor.work);
                app_mode.is_in_usb_mode = true;
                app_global_data.is_app_enabled_dlps = true;
                return;
            }
        }
        else
        {
            usb_in_high_vol_num = 0;
        }
        usb_in_debonce_timer_num ++;
         LOG_DBG("[usb_mode_monitor_debounce_timeout_cb] usb_in_debonce_timer_num is %d",
                        usb_in_debonce_timer_num);
        if (usb_in_debonce_timer_num < 40) /* 2s */
        {
            k_timer_start(&usb_mode_monitor_timer, K_MSEC(50),K_NO_WAIT);
        }
        else
        {
            if(app_mode.is_in_usb_mode)
            {

            }
            else
            {
                is_usb_in_debonce_check = false;
                gpio_pin_interrupt_configure_dt(&detect_usb, GPIO_INT_LEVEL_HIGH);
                usb_mode_monitor_trigger_level = GPIO_PIN_LEVEL_HIGH;
            }
        }
    }
    else if(is_usb_out_debonce_check)
    {
        app_global_data.is_app_enabled_dlps = false;
        if(usb_pin_polarity_status == GPIO_PIN_LEVEL_LOW)
        {
            LOG_DBG("[usb_mode_monitor_debounce_timeout_cb]: usb_out_low_vol is %d",usb_out_low_vol_num);
            usb_out_low_vol_num ++;
            if (usb_out_low_vol_num >= USB_OUT_DETECT_NUM) /* 300ms */
            {
                /* usb out */
                usb_in_high_vol_num = 0;
                is_usb_out_debonce_check = false;
                gpio_pin_interrupt_configure_dt(&detect_usb, GPIO_INT_LEVEL_HIGH);
                usb_mode_monitor_trigger_level = GPIO_PIN_LEVEL_HIGH;
                struct zmk_mode_monitor_event ev = {
                    .app_cur_mode = USB_MODE,
                    .state_changed = 0
                };
                k_msgq_put(&zmk_mode_monitor_msgq, &ev, K_NO_WAIT);
                k_work_submit(&mm_msg_processor.work);
                app_global_data.is_app_enabled_dlps = true;
                return;
            }
        }
        else
        {
            usb_out_low_vol_num = 0;
        }
        usb_out_debonce_timer_num ++;
        if (usb_out_debonce_timer_num < 60) /* 3s */
        {
            k_timer_start(&usb_mode_monitor_timer, K_MSEC(50),K_NO_WAIT);
        }
        else
        {
            is_usb_out_debonce_check = false;
            gpio_pin_interrupt_configure_dt(&detect_usb, GPIO_INT_LEVEL_LOW);
            usb_mode_monitor_trigger_level = GPIO_PIN_LEVEL_LOW;
        }
    }
}

void cap_led_on(void)
{
    gpio_pin_set(cap_led.port, cap_led.pin, 0);
}

void cap_led_off(void)
{
    gpio_pin_set(cap_led.port, cap_led.pin, 1);
}

void num_led_on(void)
{
    gpio_pin_set(num_led.port, num_led.pin, 0);
}

void num_led_off(void)
{
    gpio_pin_set(num_led.port, num_led.pin, 1);
}

void keyboard_switch_os(void)
{
    LOG_DBG("keyboard_switch_os");
    zmk_keymap_layer_toggle(MACOS);
}
#if IS_ENABLED(CONFIG_PM)
/**
 * @brief  CPU can enter wfi or dlps with checking all module status pass
 * @param  None
 * @return None
 */
void pm_check_status_before_enter_wfi_or_dlps(void)
{
    if (!is_check_status_before_enter_wfi)
    {
        power_mode_resume();
        is_check_status_before_enter_wfi = true;
    }
}

/**
 * @brief  CPU enter wfi without checking all module status
 * @param  None
 * @return None
 */
void pm_no_check_status_before_enter_wfi(void)
{
    if (is_check_status_before_enter_wfi)
    {
        power_mode_pause();
        is_check_status_before_enter_wfi = false;
    }
}
#endif

SYS_INIT(zmk_mode_monitor_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);