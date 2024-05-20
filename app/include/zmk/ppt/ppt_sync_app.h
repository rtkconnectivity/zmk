/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <ppt_sync.h>
#include <zmk/board.h>

/*============================================================================*
 *                              Macros
 *============================================================================*/
#define PPT_KEYBOARD_DATA_MAX_SIZE  9+1

#define PPT_DATA_MAX_SIZE           PPT_KEYBOARD_DATA_MAX_SIZE

#define PPT_DATA_MAX_SEQ_NUM        0x1f

/*============================================================================*
 *                              Variables
 *============================================================================*/
typedef enum
{
    V_WHEEL_UP      = 0x20,
    V_WHEEL_DOWN    = 0x40,
    H_WHEEL_UP      = 0x60,
    H_WHEEL_DOWN    = 0x80,
} wheel_data_t;

typedef enum
{
    X_Y_SIZE_0_BIT  = 0,
    X_Y_SIZE_2_BIT  = 1,
    X_Y_SIZE_4_BIT  = 2,
    X_Y_SIZE_6_BIT  = 3,
    X_Y_SIZE_8_BIT  = 4,
    X_Y_SIZE_10_BIT = 5,
    X_Y_SIZE_12_BIT = 6,
    X_Y_SIZE_16_BIT = 7,
} x_y_size_t;

typedef enum
{
    /* >= SYNC_OPCODE_APP_START_VALUE, <= 0x3f */
    SYNC_OPCODE_KEYBOARD    = 0x10,
    SYNC_OPCODE_CONSUMER    = 0x11,
    SYNC_OPCODE_VENDOR      = 0x12,
    SYNC_OPCODE_DPI         = 0x20,
    SYNC_OPCODE_REPORT_RATE = 0x21,
} sync_app_opcode_t;

typedef union
{

    /* zero and is_mouse_data_header must be 0, header_byte must <= 0x3f*/
    union
    {
        uint8_t value;
        struct
        {
            sync_app_opcode_t app_opcode: 6;
            uint8_t is_mouse_data_header: 1;
            uint8_t zero: 1;
        } bit;
    } other_data_header;

    /* zero must be 0, is_mouse_data_header must be 1, is_multi_data must be 0 */
    union
    {
        uint8_t value;
        struct
        {
            uint8_t seq_num: 1;
            x_y_size_t x_y_bit_size: 3;
            uint8_t button_wheel_valid: 1;
            uint8_t is_multi_data: 1;
            uint8_t is_mouse_data_header: 1;
            uint8_t zero: 1;
        } bit;
    } mouse_single_data_header;

    /* zero must be 0, is_mouse_data_header must be 1, is_multi_data must be 1 */
    union
    {
        uint16_t value;
        struct
        {
            uint8_t seq_num : 5;
            x_y_size_t x2_y2_bit_size: 3;
            x_y_size_t x1_y1_bit_size: 3;
            uint8_t button2_wheel2_valid: 1;
            uint8_t button1_wheel1_valid: 1;
            uint8_t is_multi_data: 1;
            uint8_t is_mouse_data_header: 1;
            uint8_t zero: 1;
        } bit;
    } mouse_multi_data_header;
} ppt_sync_app_header_t;

typedef struct
{
    sync_msg_send_cb_t msg_send_cb;
    sync_bond_info_t bond_info;
    uint8_t msg_retrans_count;
    sync_bond_info_t bond_info_backup;
} ppt_sync_app_t;

/*============================================================================*
 *                              Functions Declaration
 *============================================================================*/
void sync_msg_reg_send_cb(sync_msg_send_cb_t cb);
void ppt_sync_init(sync_role_t role);
void ppt_sync_enable(void);
bool ppt_check_is_bonded(void);
bool ppt_pair(void);
bool ppt_reconnect(void);
void ppt_stop_sync(void);
bool ppt_clear_bond_info(void);
sync_err_code_t ppt_app_send_data(sync_msg_type_t type, uint8_t msg_retrans_count,
                                  uint8_t *data, uint16_t len);

