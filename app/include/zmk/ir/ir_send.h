/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/sys/util.h>
#include <zephyr/types.h>
#include "zmk/board.h"
 
/*============================================================================*
 *                          IR Send config
 *============================================================================*/
#define IR_SEND_WAVE_MAX_LEN            70
#define IR_REPEAT_CODE_MAX_LEN          3

#define IR_DUTY_CYCLE                   3

#define PULSE_HIGH                      ((uint32_t)0x80000000)
#define PULSE_LOW                       0x0
#define LOG_HIGH                        1
#define LOG_LOW                         0
#define IR_DATA_MSK                     0x7fffffffUL

#define IR_TX_FIFO_THR_LEVEL            2

#define IR_EVENT_QUEUE_SIZE	4

#define KEY_CODE_TABLE_SIZE 20

typedef enum
{
    IR_SEND_KEY_PRESS,
    IR_SEND_KEY_RELEASE,
} T_IR_SEND_KEY_STATE;

typedef enum
{
    IR_SEND_IDLE,
    IR_SEND_CAMMAND,
    IR_SEND_CAMMAND_COMPLETE,
    IR_SEND_REPEAT_CODE,
    IR_SEND_REPEAT_CODE_COMPLETE,
} T_IR_SEND_STATE;

typedef enum
{
    IRDA_ERROR        = -1,
    IRDA_SUCCEED      = 0,
    IRDA_HEADER_ERROR = 1,
    IRDA_DATA_ERROR
} T_IRDA_RET;

typedef enum
{
	IO_MSG_TYPE_IR_LEARN_DATA,       /**< ir learn data message*/
    IO_MSG_TYPE_IR_LEARN_STOP,       /**< ir learn stop message*/
    IO_MSG_TYPE_IR_START_SEND_REPEAT_CODE, /**< ir send repeat code message*/
    IO_MSG_TYPE_IR_SEND_COMPLETE,    /**< ir send complete message*/
}T_IR_MSG_TYPE;

typedef struct
{
    float   carrier_frequency_hz;
    float    duty_cycle;
    uint32_t ir_send_buf[IR_SEND_WAVE_MAX_LEN];
    uint32_t send_buf_len;
    uint32_t ir_repeat_code_buf[IR_REPEAT_CODE_MAX_LEN];
    uint32_t repeat_buf_len;
    uint32_t command_time_ms;
    uint32_t repeat_code_time_ms;
} T_IR_SEND_PARA;

typedef struct
{
    T_IR_SEND_STATE  ir_send_state;             /*ir send state*/
    T_IR_SEND_PARA   *p_ir_send_data;
} T_IR_SEND_STRUCT;

typedef enum
{
    NEC_PROTOCOL     = 0,
} T_IR_PROTOCOL;

/* define the key types */
typedef enum
{
    KEY_TYPE_NONE       = 0x00,  /* none key type */
    KEY_TYPE_BLE_ONLY   = 0x01,  /* only BLE key type */
    KEY_TYPE_IR_ONLY    = 0x02,  /* only IR key type */
    KEY_TYPE_BLE_OR_IR  = 0x03,  /* BLE or IR key type */
} T_KEY_TYPE_DEF;

/* define the struct of key code */
typedef struct
{
    T_KEY_TYPE_DEF key_type;
    uint8_t ir_key_code;
    uint8_t hid_usage_page;
    uint32_t hid_usage_id;
} T_KEY_CODE_DEF;

extern const struct device *const ir_dev;

void ir_send_msg_proc(struct k_work *item);
void ir_send_key_press_handle(uint32_t ir_key_index);
void ir_send_key_release_handle(void);

bool ir_send_is_working(void);
// bool ir_send_check_dlps(void);
// void ir_send_enter_dlps_config(void);
// void ir_send_exit_dlps_config(void);
T_IR_SEND_STATE ir_send_get_current_state(void);
bool ir_send_module_init(T_IR_SEND_PARA *p_ir_send_para);
bool ir_send_command_start(void);
bool ir_send_repeat_code_start(void);
void ir_send_exit(void);
uint32_t ir_send_convert_to_carrier_cycle(uint32_t time, uint32_t freq);

int ir_test(void);