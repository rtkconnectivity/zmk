/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/sys/util.h>
#include <zephyr/types.h>

typedef struct
{
    uint8_t   led_type_index;
    uint8_t   led_color_mask_bit[3];
} T_LED_EVENT_STG;
/*led color mask bit : 32bits */
#define   LED_COLOR_IDLE_MASK                   {0, 0, 0}
#define   LED_COLOR_RED_MASK                    {1, 0, 0}
#define   LED_COLOR_GREEN_MASK                  {0, 1, 0}
#define   LED_COLOR_BLUE_MASK                   {0, 0, 1}
#define   LED_COLOR_YELLOW_MASK                 {1, 1, 0}
#define   LED_COLOR_CYAN_MASK                   {0, 1, 1}
#define   LED_COLOR_PURPLR_MASK                 {1, 0, 1}
#define   LED_COLOR_WHITE_MASK                  {1, 1, 1}

#define LED_LOOP_BITS_IDLE 20
#define LED_LOOP_BITS_20_TIMES 20
typedef enum
{
    LED_TYPE_IDLE,
    LED_TYPE_BLINK_CHARGING,
    LED_TYPE_BLINK_LOW_POWER,
	LED_TYPE_BLINK_MID_POWER,
	LED_TYPE_BLINK_HIGH_POWER,
    LED_TYPE_MAX
} LED_TYPE_ENUM;

void led_event_handler(uint8_t percent);