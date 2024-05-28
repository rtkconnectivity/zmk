/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

#include <zmk/ppt/keyboard_ppt_app.h>
#include <zmk/mode_monitor.h>
#include <zmk/ppt.h>
#include <zmk/event_manager.h>
#include <zmk/events/ppt_conn_state_changed.h>
#include "rtl_pinmux.h"
#include "trace.h"


LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static void no_act_disconn_timer_callback(struct k_timer *p_timer);
static K_TIMER_DEFINE(no_act_disconn_timer, no_act_disconn_timer_callback, NULL);

/*============================================================================*
 *                             Macro
 *============================================================================*/
/* uint: dbm */
#if FEATURE_SUPPORT_APP_CFG_PPT_TX_POWER
#define PPT_TX_POWER_DBM_MAX        4
#define PPT_TX_POWER_DBM_MIN        4
#endif
/*============================================================================*
 *                             Global Variables
 *============================================================================*/
T_APP_PPT_GLOBAL_DATA ppt_app_global_data;

/*============================================================================*
 *                             Local Variables
 *============================================================================*/
static uint32_t usb_report_rate_level[REPORT_RATE_LEVEL_NUM] = {USB_REPORT_RATE_LEVEL_0, USB_REPORT_RATE_LEVEL_1, USB_REPORT_RATE_LEVEL_2};
static uint32_t ppt_report_rate_level[REPORT_RATE_LEVEL_NUM] = {PPT_REPORT_RATE_LEVEL_0, PPT_REPORT_RATE_LEVEL_1, PPT_REPORT_RATE_LEVEL_2};
/*============================================================================*
 *                             Functions Declaration
 *============================================================================*/
static void ppt_app_send_msg_cb(sync_msg_type_t type, uint8_t *data, uint16_t len,
                                sync_send_info_t *info);
void zmk_ppt_sync_event_cb(sync_event_t event);

static void raise_ppt_status_changed_event(struct k_work *_work) {
    raise_zmk_ppt_conn_state_changed(
        (struct zmk_ppt_conn_state_changed){.conn_state = zmk_ppt_get_conn_state()});
}

K_WORK_DEFINE(ppt_status_notifier_work, raise_ppt_status_changed_event);
/*============================================================================*
*                              Local Functions
*============================================================================*/
/******************************************************************
 * @brief  ppt_app_receive_msg_cb
 * @param  p_data - pointer to receive data.
 * @param  len - data length.
 * @param  info - receive information
 * @return none
 * @retval void
 */
static void ppt_app_receive_msg_cb(uint8_t *p_data, uint16_t len, sync_receive_info_t *info)
{
    DBG_DIRECT("ppt_app_receive_msg_cb");
}

/******************************************************************
 * @brief  ppt_app_send_msg_cb
 * @param  type - 2.4g message type.
 * @param  p_data - pointer to send data.
 * @param  len - data length
 * @param  info - sending information
 * @return none
 * @retval void
 */
static void ppt_app_send_msg_cb(sync_msg_type_t type, uint8_t *p_data, uint16_t len,
                                sync_send_info_t *info)
{
    DBG_DIRECT("ppt_app_send_msg_cb, type: %d, len: %d, send_result: %d!", type, len,
                    info->res);
}

/******************************************************************
 * @brief  zmk_ppt_sync_event_cb
 * @param  event - 2.4g sync event
 * @return none
 * @retval void
 */
void zmk_ppt_sync_event_cb(sync_event_t event)
{
    switch (event)
    {
    case SYNC_EVENT_PAIRED:
        {
            DBG_DIRECT("[zmk_ppt_sync_event_cb] SYNC_EVENT_PAIRED");
            ppt_app_global_data.keyboard_ppt_status = KEYBOARD_PPT_STATUS_PAIRED;
            ppt_app_global_data.is_ppt_bond = true;
            ppt_app_global_data.pair_time_count = 0;

            ppt_app_global_data.send_data_index = 0;
        }
        break;
    case SYNC_EVENT_PAIR_TIMEOUT:
        {
            DBG_DIRECT("[zmk_ppt_sync_event_cb] SYNC_EVENT_PAIR_TIMEOUT");
            if (ppt_app_global_data.pair_time_count < PPT_PAIR_TIME_MAX_COUNT)
            {
                keyboard_ppt_pair();
                ppt_app_global_data.pair_time_count++;
            }
            else
            {
                ppt_app_global_data.pair_time_count = 0;
                ppt_app_global_data.keyboard_ppt_status = KEYBOARD_PPT_STATUS_IDLE;
            }
        }
        break;
    case SYNC_EVENT_CONNECTED:
        {
            DBG_DIRECT("[zmk_ppt_sync_event_cb] SYNC_EVENT_CONNECTED");
            ppt_app_global_data.keyboard_ppt_status = KEYBOARD_PPT_STATUS_CONNECTED;

#if FEATURE_SUPPORT_NO_ACTION_DISCONN
            if (ppt_app_global_data.reconnect_time_count <= 2)
            {
                k_timer_start(&no_act_disconn_timer, K_MSEC(NO_ACTION_DISCON_TIMEOUT),K_NO_WAIT);

            }
#endif
            ppt_app_global_data.reconnect_time_count = 0;
        }
        break;
    case SYNC_EVENT_CONNECT_TIMEOUT:
        {
            DBG_DIRECT("[zmk_ppt_sync_event_cb] SYNC_EVENT_CONNECT_TIMEOUT");
            ppt_app_global_data.keyboard_ppt_status = KEYBOARD_PPT_STATUS_IDLE;

            if (ppt_app_global_data.reconnect_time_count < PPT_RECONNECT_TIME_MAX_COUNT)
            {
                keyboard_ppt_reconnect();
                ppt_app_global_data.reconnect_time_count++;
            }
            else
            {

            }
        }
        break;
    case SYNC_EVENT_CONNECT_LOST:
        {
            DBG_DIRECT("[zmk_ppt_sync_event_cb] SYNC_EVENT_CONNECT_LOST");
            ppt_app_global_data.keyboard_ppt_status = KEYBOARD_PPT_STATUS_IDLE;
            if (ppt_app_global_data.is_ppt_bond)
            {
                if (ppt_app_global_data.reconnect_time_count < PPT_RECONNECT_TIME_MAX_COUNT)
                {
                    keyboard_ppt_reconnect();
                    ppt_app_global_data.reconnect_time_count++;
                }
            }
            else if (ppt_app_global_data.pair_time_count < PPT_PAIR_TIME_MAX_COUNT)
            {
                keyboard_ppt_pair();
                ppt_app_global_data.pair_time_count++;
            }
#if FEATURE_SUPPORT_NO_ACTION_DISCONN
            /* stop no_act_disconn_timer if disconnection detected */
            k_timer_stop(&no_act_disconn_timer);
#endif
        }
        break;
    default:
        break;
    }
    k_work_submit(&ppt_status_notifier_work);
}

/*============================================================================*
*                              Global Functions
*============================================================================*/
/******************************************************************
 * @brief  keyboard_ppt_pair
 * @param  none
 * @return none
 * @retval void
 */
void keyboard_ppt_pair(void)
{
    DBG_DIRECT("keyboard ppt pair");
    if (true == ppt_pair())
    {
        ppt_app_global_data.keyboard_ppt_status = KEYBOARD_PPT_STATUS_PAIRING;
    }
}

/******************************************************************
 * @brief  keyboard_ppt_reconnect
 * @param  none
 * @return none
 * @retval void
 */
void keyboard_ppt_reconnect(void)
{
    if (true == ppt_reconnect())
    {
        ppt_app_global_data.keyboard_ppt_status = KEYBOARD_PPT_STATUS_CONNECTING;

    }
}

/******************************************************************
 * @brief  keyboard_stop_sync
 * @param  none
 * @return none
 * @retval void
 */
void keyboard_ppt_stop_sync(void)
{
    ppt_stop_sync();
    ppt_app_global_data.keyboard_ppt_status = KEYBOARD_PPT_STATUS_IDLE;
#if FEATURE_SUPPORT_NO_ACTION_DISCONN
    /* stop no_act_disconn_timer if disconnection detected */
    k_timer_stop(&no_act_disconn_timer);
#endif
}

/******************************************************************
 * @brief  keyboard_ppt_init
 * @param  none
 * @return none
 * @retval void
 */
void keyboard_ppt_init(void)
{
    ppt_sync_init(SYNC_ROLE_MASTER);
    sync_msg_reg_receive_cb(ppt_app_receive_msg_cb);
    sync_msg_reg_send_cb(ppt_app_send_msg_cb);
    sync_event_cb_reg(zmk_ppt_sync_event_cb);
    ppt_app_global_data.keyboard_ppt_status = KEYBOARD_PPT_STATUS_IDLE;
    ppt_app_global_data.is_ppt_bond = ppt_check_is_bonded();
    DBG_DIRECT("ppt_app_global_data.is_ppt_bond = %d", ppt_app_global_data.is_ppt_bond);
    uint32_t ppt_interval_time_us = PPT_REPORT_RATE_LEVEL_0 ;
    /* set 2.4g connection interval */
    sync_time_set(SYNC_TIME_PARAM_CONNECT_INTERVAL, ppt_interval_time_us);
    DBG_DIRECT("ppt_interval_time_us = %d us", ppt_interval_time_us);

    /* set 2.4g connection heart beat interval */
    sync_master_set_hb_param(2, PPT_DEFAULT_HEARTBEAT_INTERVAL_TIME, 0);

    /* set ppt tx_power */
#if FEATURE_SUPPORT_APP_CFG_PPT_TX_POWER
    sync_tx_power_set(false, PPT_TX_POWER_DBM_MAX, PPT_TX_POWER_DBM_MIN);
#endif
    /*Different message types have different queue size*/
    uint8_t msg_quota[SYNC_MSG_TYPE_NUM] = {0, 2, 2, 2};
    sync_msg_set_quota(msg_quota);

#if !ENABLE_2_4G_LOG
    sync_log_set(0, false);
#endif
    sync_log_set(0,true);
}

/******************************************************************
 * @brief    get_report_rate_level_by_index
 * @param    keyboard mode - keyboard mode type
 * @param    index - report rate index
 * @return   report rate value
 * @retval   uint32_t
 */
uint32_t get_report_rate_level_by_index(uint8_t keyboard_mode, uint32_t index)
{
    if (keyboard_mode == USB_MODE)
    {
        return usb_report_rate_level[index];
    }
    else if (keyboard_mode == PPT_MODE)
    {
        return ppt_report_rate_level[index];
    }
    return 0;
}

/******************************************************************
 * @brief  enalbe 2.4g proprietary for dongle
 * @param  none
 * @return none
 * @retval void
 */
void keyboard_ppt_enable(void)
{
    ppt_app_global_data.pair_time_count = 0;
    ppt_app_global_data.reconnect_time_count = 0;
    ppt_sync_enable();
    if (ppt_app_global_data.is_ppt_bond)
    {
        ppt_app_global_data.reconnect_time_count = 1;
        keyboard_ppt_reconnect();
    }
}

/******************************************************************
 * @brief  init 2.4g proprietary global data
 * @param  none
 * @return none
 * @retval void
 */
void app_init_ppt_global_data(void)
{
    memset(&ppt_app_global_data, 0, sizeof(ppt_app_global_data));
    ppt_app_global_data.keyboard_ppt_status = KEYBOARD_PPT_STATUS_IDLE;
}

/******************************************************************
 * @brief  send keyboard data by ppt
 * @param  type - send type
 * @param  msg_retrans_count - retrans count
 * @param  keyboard_data
 * @return none
 * @retval void
 */
bool keyboard_app_ppt_send_data(sync_msg_type_t type, uint8_t msg_retrans_count,
                                T_KEYBOARD_DATA keyboard_data)
{
    sync_err_code_t ret;
    if (keyboard_data.hid_report_type == HID_REPORT_KEYBOARD)
    {
        uint8_t tx_keyboard_data[KEYBOARD_DATA_SIZE + 1] = {0};
        tx_keyboard_data[0] = SYNC_OPCODE_KEYBOARD;
        memcpy(&tx_keyboard_data[1], keyboard_data.keyboard_common_data, KEYBOARD_DATA_SIZE);

        ret = ppt_app_send_data(type, msg_retrans_count, tx_keyboard_data, KEYBOARD_DATA_SIZE + 1);
        if (ret == SYNC_ERR_CODE_SUCCESS)
        {
            DBG_DIRECT("[key_handle_notify_hid_usage_buffer] send common_keyboard_buffer: 0x %b",
                            TRACE_BINARY(sizeof(tx_keyboard_data), tx_keyboard_data));
            return true;
        }
        else
        {
            DBG_DIRECT("keyboard app ppt send data fail! key store in queue! error reason = %d", ret);
        }
    }
    if (keyboard_data.hid_report_type == HID_REPORT_CONSUMER)
    {
        uint8_t tx_comsumer_data[CONSUMER_DATA_SIZE + 1] = {0};
        tx_comsumer_data[0] = SYNC_OPCODE_CONSUMER;
        memcpy(&tx_comsumer_data[1], keyboard_data.keyboard_consumer_data, CONSUMER_DATA_SIZE);

        ret = ppt_app_send_data(type, msg_retrans_count, tx_comsumer_data, CONSUMER_DATA_SIZE + 1);
        if (ret == SYNC_ERR_CODE_SUCCESS)
        {
            DBG_DIRECT("[key_handle_notify_hid_usage_buffer] send fn_keyboard_buffer: 0x %b",
                            TRACE_BINARY(sizeof(tx_comsumer_data), tx_comsumer_data));
            return true;
        }
        else
        {
            DBG_DIRECT("keyboard app ppt send data fail! error reason = %d", ret);
        }
    }
    if (keyboard_data.hid_report_type == HID_REPORT_VENDOR)
    {
        uint8_t tx_vendor_data[VENDOR_DATA_SIZE + 1] = {0};
        tx_vendor_data[0] = SYNC_OPCODE_VENDOR;
        memcpy(&tx_vendor_data[1], keyboard_data.keyboard_vendor_data, VENDOR_DATA_SIZE);
        ret = ppt_app_send_data(type, msg_retrans_count, tx_vendor_data, VENDOR_DATA_SIZE + 1);
        if (ret == SYNC_ERR_CODE_SUCCESS)
        {
            DBG_DIRECT("[key_handle_notify_hid_usage_buffer] send vendor_buffer: 0x %x",
                            TRACE_BINARY(sizeof(tx_vendor_data), tx_vendor_data));
            return true;
        }
        else
        {
            DBG_DIRECT("keyboard app ppt send data fail! error reason = %d", ret);
        }
    }
    return false;
}

#if FEATURE_SUPPORT_NO_ACTION_DISCONN
/**
 * @brief  No action and disconnect timer callback
 *         it is used to terminate connection after timeout
 * @param  p_timer - timer handler
 * @return None
 */
void no_act_disconn_timer_callback(struct k_timer *p_timer)
{
    if (app_mode.is_in_ppt_mode)
    {
        if (ppt_app_global_data.mouse_ppt_status == KEYBOARD_PPT_STATUS_CONNECTED)
        {
            DBG_DIRECT("[KEYBOARD] Idle No Action Timeout, Disconnect.");
            keyboard_ppt_stop_sync();
        }
    }
}
#endif

bool zmk_ppt_is_ready(void)
{
    return zmk_ppt_get_conn_state() == ZMK_PPT_CONN;
}

enum zmk_ppt_conn_state zmk_ppt_get_conn_state(void) {
    DBG_DIRECT("state: %d", ppt_app_global_data.keyboard_ppt_status);
    switch (ppt_app_global_data.keyboard_ppt_status) {
    case KEYBOARD_PPT_STATUS_PAIRED:           /* start sync but not paired */
    case KEYBOARD_PPT_STATUS_CONNECTED:         /* connected success status */
    case KEYBOARD_PPT_STATUS_LOW_POWER:
        return ZMK_PPT_CONN;
    
    case KEYBOARD_PPT_STATUS_PAIRING:
    case KEYBOARD_PPT_STATUS_CONNECTING:
        return ZMK_PPT_CONN_NONE;

    default:
        return ZMK_PPT_CONN_NONE;
    }
}

void zmk_ppt_init(void)
{
    DBG_DIRECT("zmk ppt init and start pair");
    keyboard_ppt_init();
    keyboard_ppt_enable();
    keyboard_ppt_pair();
}
