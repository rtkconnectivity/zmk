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
#include <zmk/leds_pwm_driver.h>
#include <zephyr/drivers/gpio.h>

//LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define LED_PWM_NODE_ID	 DT_COMPAT_GET_ANY_STATUS_OKAY(pwm_leds)

static struct gpio_dt_spec leds_pwron = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_leds), pwr_gpios);

const char *led_label[] = {
	DT_FOREACH_CHILD_SEP_VARGS(LED_PWM_NODE_ID, DT_PROP_OR, (,), label, NULL)
};

const int num_leds = ARRAY_SIZE(led_label);
const struct device *led_pwm = DEVICE_DT_GET(LED_PWM_NODE_ID);

static T_LED_EVENT_STG led_event_arr[LED_TYPE_MAX] =
{
    {LED_TYPE_IDLE,                    LED_COLOR_IDLE_MASK,   },
    {LED_TYPE_BLINK_CHARGING,          LED_COLOR_RED_MASK,    },
    {LED_TYPE_BLINK_LOW_POWER,         LED_COLOR_RED_MASK,    },
    {LED_TYPE_BLINK_MID_POWER,         LED_COLOR_YELLOW_MASK, },
    {LED_TYPE_BLINK_HIGH_POWER,        LED_COLOR_GREEN_MASK,  },
};

void led_event_handler(uint8_t percent)
{
	uint8_t i,j;
    uint16_t led_type;
    if(percent <= 30)
    {
        led_type = LED_TYPE_BLINK_LOW_POWER;
    }
    else if(percent > 30 && percent <= 60)
    {
        led_type = LED_TYPE_BLINK_MID_POWER;
    }
    else if(percent > 60 && percent <= 100)
    {
        led_type = LED_TYPE_BLINK_HIGH_POWER;
    }
	else
	{
		led_type = LED_TYPE_IDLE;
	}
	for(j=0; j<num_leds; j++)
	{
		led_off(led_pwm,j);
	}
	
	for(i=0; i < LED_TYPE_MAX; i++)
	{
		if(led_type == led_event_arr[i].led_type_index)
		{
			break;
		}
	}
	
	if(led_event_arr[i].led_color_mask_bit[0] == 1)
	{
		led_on(led_pwm,0);
	}
	if(led_event_arr[i].led_color_mask_bit[1] == 1)
	{
		led_on(led_pwm,1);
	}
	if(led_event_arr[i].led_color_mask_bit[2] == 1)
	{
		led_on(led_pwm,2);
	}
	gpio_pin_configure_dt(&leds_pwron, GPIO_OUTPUT_HIGH);
}