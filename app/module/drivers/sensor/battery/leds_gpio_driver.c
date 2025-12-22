/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/gpio.h>
#include <zmk/leds_gpio_driver.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct gpio_dt_spec leds_pwron = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), pwr_gpios);
static struct gpio_dt_spec cap_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), cap_gpios);
static struct gpio_dt_spec num_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), num_gpios);
static struct gpio_dt_spec ppt_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), ppt_gpios);
static struct gpio_dt_spec bt_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), ble_gpios);

static void led_gpio_ctrl_timeout_cb(struct k_timer *timer);
static K_TIMER_DEFINE(leds_gpio_ctrl_timer, led_gpio_ctrl_timeout_cb, NULL);

T_GPIO_LEDS_GLOBAL_DATA gpio_leds_global_data = {.led_flag = false};

void led_blink_start(struct gpio_dt_spec *led, uint8_t cnt)
{
    gpio_leds_global_data.led = led;
    gpio_leds_global_data.blink_cnt = cnt;
    while(gpio_leds_global_data.blink_cnt >= 0) {
        k_timer_start(&leds_gpio_ctrl_timer, K_MSEC(LED_GPIO_BLINK_MS), K_NO_WAIT);
        gpio_leds_global_data.blink_cnt--;
    }
}

void led_blink_exit(void)
{
    k_timer_stop(&leds_gpio_ctrl_timer);
}

void gpio_led_on(struct gpio_dt_spec *led)
{
    gpio_pin_set(led->port, led->pin, 0);
    LOG_DBG("config gpio led port %s, pin %d",led->port->name,led->pin);
}

void gpio_led_off(struct gpio_dt_spec *led)
{
    gpio_pin_set(led->port, led->pin, 1);
}

static void led_gpio_ctrl_timeout_cb(struct k_timer *timer) {

    if(gpio_leds_global_data.led_flag == false) {
        gpio_pin_set(gpio_leds_global_data.led->port, gpio_leds_global_data.led->pin, 1);
        gpio_leds_global_data.led_flag = true;
    } else if(gpio_leds_global_data.led_flag == true) {
        gpio_pin_set(gpio_leds_global_data.led->port, gpio_leds_global_data.led->pin, 0);
        gpio_leds_global_data.led_flag = false;
    }

}

static void leds_gpio_driver_init(void) {
    /* gpio leds config */
    LOG_DBG("leds gpio driver init");
    gpio_pin_interrupt_configure_dt(&leds_pwron, GPIO_INT_DISABLE);

    gpio_pin_configure_dt(&leds_pwron, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&cap_led, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&num_led, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&bt_led, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&ppt_led, GPIO_OUTPUT_HIGH);
}

SYS_INIT(leds_gpio_driver_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);