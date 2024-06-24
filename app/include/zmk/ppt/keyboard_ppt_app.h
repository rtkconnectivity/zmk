/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#include <zmk/ppt/ppt_sync_app.h>

/*============================================================================*
 *                              Macros
 *============================================================================*/
#define PPT_DEFAULT_INTERVAL_TIME           250 /* 250us */
#define PPT_DEFAULT_HEARTBEAT_INTERVAL_TIME 250000 /* 250ms */

#define PPT_PAIR_TIME_MAX_COUNT         30 /* 30s */
#define PPT_RECONNECT_TIME_MAX_COUNT    4 /* 4s */

#define KEYBOARD_DATA_SIZE                  3
#define CONSUMER_DATA_SIZE                  2
#define VENDOR_DATA_SIZE                    4

#define NO_ACTION_DISCON_TIMEOUT_MS         1000
#define REPORT_RATE_LEVEL_NUM               3
#define USB_REPORT_RATE_LEVEL_0             2000
#define USB_REPORT_RATE_LEVEL_1             4000
#define USB_REPORT_RATE_LEVEL_2             8000

#define PPT_REPORT_RATE_LEVEL_0             1000
#define PPT_REPORT_RATE_LEVEL_1             2000
#define PPT_REPORT_RATE_LEVEL_2             4000
#define PPT_REPORT_RATE_DEFAULT_INDEX       2
#define USB_REPORT_RATE_DEFAULT_INDEX       2

/*============================================================================*
 *                              Variables
 *============================================================================*/
typedef enum
{
    KEYBOARD_PPT_STATUS_IDLE = 0,          /* idle status */
    KEYBOARD_PPT_STATUS_PAIRING,           /* start sync but not paired */
    KEYBOARD_PPT_STATUS_PAIRED,            /* paired success status */
    KEYBOARD_PPT_STATUS_CONNECTING,        /* start sync but not connected */
    KEYBOARD_PPT_STATUS_CONNECTED,         /* connected success status */
    KEYBOARD_PPT_STATUS_LOW_POWER,         /* low power mode*/
} T_KEYBOARD_PPT_STATUS;

typedef enum
{
    PPT_DISCONN_REASON_IDLE = 0,
    PPT_DISCONN_REASON_PAIRING,
    PPT_DISCONN_REASON_TIMEOUT,
    PPT_DISCONN_REASON_PAIR_FAILED,
    PPT_DISCONN_REASON_LOW_POWER,
    PPT_DISCONN_REASON_UART_CMD,
} T_PPT_DISCONN_REASON;

typedef enum
{
    HID_REPORT_KEYBOARD = 0x00,
    HID_REPORT_CONSUMER = 0x01,
    HID_REPORT_VENDOR = 0x02,
} T_KEYBOARD_REPORT_TYPE;

typedef struct
{
    T_KEYBOARD_REPORT_TYPE hid_report_type;
    uint8_t keyboard_common_data[KEYBOARD_DATA_SIZE];       /*common key, send by HID_REPORT_KEYBOARD */
    uint8_t keyboard_consumer_data[CONSUMER_DATA_SIZE];        /*fn combine key, send by HID_REPORT_CONSUMER*/
    uint8_t keyboard_vendor_data[VENDOR_DATA_SIZE];      /*vendor key, send by HID_REPORT_VENDOR */
} T_KEYBOARD_DATA;

typedef struct t_app_ppt_global_data
{
    bool is_ppt_bond; /* to indicate whether keyboard is bond for 2.4g proprietary */
    T_KEYBOARD_PPT_STATUS keyboard_ppt_status;
    uint8_t pair_time_count;
    uint8_t reconnect_time_count;
    uint32_t send_data_index;
    uint8_t send_buf_data_num;
} T_APP_PPT_GLOBAL_DATA;
extern T_APP_PPT_GLOBAL_DATA ppt_app_global_data;

/*============================================================================*
 *                              Functions Declaration
 *============================================================================*/
void app_init_ppt_global_data(void);
void keyboard_ppt_init(void);
void keyboard_ppt_enable(void);
void keyboard_ppt_pair(void);
void keyboard_ppt_reconnect(void);
void keyboard_ppt_stop_sync(void);
void keyboard_ppt_set_sync_interval(void);
bool keyboard_app_ppt_send_data(sync_msg_type_t type, uint8_t msg_retrans_count,
                                T_KEYBOARD_DATA keyboard_data);
enum zmk_ppt_conn_state zmk_ppt_get_conn_state(void);
bool zmk_ppt_is_ready(void);
void zmk_ppt_init(void);
#if FEAUTRE_SUPPORT_VENDOR_SHORTCUT_KEY
bool app_ppt_send_vendor_data(sync_msg_type_t type, uint8_t msg_retrans_count,
                              uint8_t *vendor_data);
#endif

