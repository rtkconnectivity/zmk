/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zmk/mode_monitor.h>
#include <zmk/board.h>
#include <zmk/mp_test/mp_test.h>
#include <zmk/mp_test/single_tone.h>
#include "rtl_bt_hci.h"
#include "reset_reason.h"
#include "mem_types.h"
#include "trace.h"

LOG_MODULE_DECLARE(mp_test, CONFIG_ZMK_LOG_LEVEL);

/*============================================================================*
 *                                  Macros
 *============================================================================*/
#if (MP_TEST_SINGLE_TONE_MODE == HCI_LAYER_SINGLE_TONE_INTERFACE)
/* open EXIT_SINGLE_TONE_TEST_WHEN_TIMEOUT to enable the function of exiting Single Tone Test when
 * Timeout */
#define EXIT_SINGLE_TONE_TEST_WHEN_TIMEOUT 0

#if EXIT_SINGLE_TONE_TEST_WHEN_TIMEOUT
#define EXIT_SINGLE_TONE_TIME (2*60*1000)    /* 2min */
#endif
#endif

/*============================================================================*
 *                              Local Variables
 *============================================================================*/
#if (MP_TEST_SINGLE_TONE_MODE == HCI_LAYER_SINGLE_TONE_INTERFACE)
#if EXIT_SINGLE_TONE_TEST_WHEN_TIMEOUT
static TimerHandle_t single_tone_exit_timer = NULL;
#endif
static bool single_tone_is_sent_start_cmd = false;
static bool single_tone_is_sent_restart_cmd = false;
static void *single_tone_task_handle;
static void *rf_test_task_handle;
#endif
/*============================================================================*
 *                              Functions Declaration
 *============================================================================*/
#if (MP_TEST_SINGLE_TONE_MODE == HCI_LAYER_SINGLE_TONE_INTERFACE)
static void single_tone_reset(uint8_t channel_num);
static void single_tone_start(uint8_t channel_num);
static bool single_tone_handle_hci_cb(T_SINGLE_TONE_EVT evt, bool status, uint8_t *p_buf,
                                      uint32_t len);
static void single_tone_task(void *p_param);
#if EXIT_SINGLE_TONE_TEST_WHEN_TIMEOUT
static void single_tone_exit_timeout_cb(TimerHandle_t p_timer) DATA_RAM_FUNCTION;
#endif
#endif

typedef enum {
	HCI_IF_EVT_OPENED = 0,     /* hci I/F open completed */
	HCI_IF_EVT_DATA_IND = 2,   /* hci I/F rx data indicated */
	HCI_IF_EVT_DATA_XMIT = 3,  /* hci I/F tx data transmitted */
	HCI_IF_EVT_ERROR = 4,      /* hci I/F error occurred */
} T_HCI_IF_EVT;

typedef bool (*P_HCI_IF_CALLBACK)(T_HCI_IF_EVT evt, bool status, uint8_t *p_buf, uint32_t len);
extern bool (*vhci_open)(P_HCI_IF_CALLBACK p_callback);
extern bool (*vhci_send)(void *p_buf, uint32_t len);
extern bool (*vhci_ack)(void *p_buf);
extern void *(*os_mem_alloc_intern)(RAM_TYPE ram_type, size_t size, const char *p_func,
									uint32_t file_line);
extern void (*os_mem_free)(void *p_block);
extern bool (*os_task_create)(void **pp_handle, const char *p_name, void (*p_routine)(void *),
                              void *p_param, uint16_t stack_size, uint16_t priority);

#if (MP_TEST_SINGLE_TONE_MODE == HCI_LAYER_SINGLE_TONE_INTERFACE)
/******************************************************************
 * @brief   reset singletone.
 * @param   channel_num - channel of singletone.
 * @return  none
 * @retval  void
 */
void single_tone_reset(uint8_t channel_num) {
    T_SINGLE_TONE_VEND_CMD_PARAMS *p_vend_cmd_params = os_mem_alloc_intern(
		RAM_TYPE_DATA_ON, sizeof(T_SINGLE_TONE_VEND_CMD_PARAMS), __func__, __LINE__);

    if (p_vend_cmd_params) {
        LOG_INF("Single Tone Reset Command!");
        p_vend_cmd_params->pkt_type = 1;
        p_vend_cmd_params->opcode = 0xfceb;
        p_vend_cmd_params->length = 6;
        p_vend_cmd_params->moduleID = 5;
        p_vend_cmd_params->subcmd = 7;
        p_vend_cmd_params->start = 0;
        p_vend_cmd_params->channle = channel_num;
        p_vend_cmd_params->power_type = 1;
        p_vend_cmd_params->tx_power = 0;

        vhci_send((uint8_t *)p_vend_cmd_params, sizeof(T_SINGLE_TONE_VEND_CMD_PARAMS));

        single_tone_is_sent_restart_cmd = true;
    }
}

/******************************************************************
 * @brief   start singletone.
 * @param   channel_num - channel of singletone.
 * @return  none
 * @retval  void
 */
void single_tone_start(uint8_t channel_num) {
    T_SINGLE_TONE_VEND_CMD_PARAMS *p_vend_cmd_params = os_mem_alloc_intern(RAM_TYPE_DATA_ON,
                                        sizeof(T_SINGLE_TONE_VEND_CMD_PARAMS),__func__,__LINE__);
    if (p_vend_cmd_params)
    {
        LOG_INF("Single Tone Start!");
        p_vend_cmd_params->pkt_type = 1;
        p_vend_cmd_params->opcode = 0xfceb;
        p_vend_cmd_params->length = 6;
        p_vend_cmd_params->moduleID = 5;
        p_vend_cmd_params->subcmd = 7;
        p_vend_cmd_params->start = 1;
        p_vend_cmd_params->channle = channel_num;
        p_vend_cmd_params->power_type = 1;
        p_vend_cmd_params->tx_power = 0;
        /** note:
		 * tx_power config:
		 * 0x00/(-20dBm), 0x60/0dBm, 0x90/3dBm,
		 * 0xA0/4dBm, 0xD0/7.5dBm(only for rtl876x)
		 */

        vhci_send((uint8_t *)p_vend_cmd_params, sizeof(T_SINGLE_TONE_VEND_CMD_PARAMS));

        single_tone_is_sent_start_cmd = true;
    }
}

/******************************************************************
 * @brief   HCI callback of single tone handler.
 * @param   evt - single tone event
 * @param   status
 * @param   p_buf - point to hci buf
 * @param   len - buf length
 * @return  result
 * @retval  true or false
 */
bool single_tone_handle_hci_cb(T_SINGLE_TONE_EVT evt, bool status, uint8_t *p_buf, uint32_t len) {
    bool result = true;
    uint8_t channel_num = 20; /* 20-2.422G ,0-2.402G, 78-2.480G*/

    LOG_INF("[single_tone_handle_hci_cb] evt is %d", evt);

    switch (evt) {
    case SINGLE_TONE_EVT_OPENED:
        if (!single_tone_is_sent_restart_cmd) {
            single_tone_reset(channel_num);
        }
        break;

    case SINGLE_TONE_EVT_CLOSED:
        break;

    case SINGLE_TONE_EVT_DATA_IND:
        vhci_ack(p_buf);
        if (!single_tone_is_sent_start_cmd) {
            single_tone_start(channel_num);
        }
        break;

    case SINGLE_TONE_EVT_DATA_XMIT:
        os_mem_free(p_buf);
        break;

    case SINGLE_TONE_EVT_ERROR:
        break;

    default:
        break;
    }
    return (result);
}

/******************************************************************
 * @brief   HCI callback of rf test handler.
 * @param   evt - single tone event
 * @param   status
 * @param   p_buf - point to hci buf
 * @param   len - buf length
 * @return  result
 * @retval  true or false
 */
bool rf_test_handle_hci_cb(T_SINGLE_TONE_EVT evt, bool status, uint8_t *p_buf, uint32_t len) {
    bool result = true;
    uint8_t channel_num = 20; /* 20-2.422G ,0-2.402G, 78-2.480G*/

    LOG_INF("[rf_test_handle_hci_cb] evt is %d", evt);

    switch (evt) {
    case SINGLE_TONE_EVT_OPENED:
        break;
    case SINGLE_TONE_EVT_CLOSED:
        break;

    case SINGLE_TONE_EVT_DATA_IND:
        vhci_ack(p_buf);
        const struct device *dev;
	    dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
	    if (!device_is_ready(dev)) {
            LOG_ERR("CDC ACM device not ready");
            return 0;
	    }
        if (uart_irq_tx_ready(dev)) {
            int send_len = uart_fifo_fill(dev, p_buf, len);
            if (send_len < len) {
                LOG_ERR("Drop %d bytes", len - send_len);
            }
            uart_irq_tx_disable(dev);
        }
        break;

    case SINGLE_TONE_EVT_DATA_XMIT:
        os_mem_free(p_buf);
        break;

    case SINGLE_TONE_EVT_ERROR:
        break;

    default:
        break;
    }
    return (result);
}

/******************************************************************
 * @brief   single tone task
 * @param   p_param
 * @return  none
 * @retval  void
 */
void single_tone_task(void *p_param) {
    k_sleep(K_MSEC(100));
    vhci_open((P_HCI_IF_CALLBACK)single_tone_handle_hci_cb);
    while (1) {
        k_sleep(K_MSEC(1000));
    }
}

void rf_test_task(void *p_param) {
    k_sleep(K_MSEC(100));
    vhci_open((P_HCI_IF_CALLBACK)rf_test_handle_hci_cb);
    while (1) {
        k_sleep(K_MSEC(1000));
    }
}

#if EXIT_SINGLE_TONE_TEST_WHEN_TIMEOUT
/******************************************************************
 * @brief   single tone exit timer callback
 * @param   p_timer
 * @return  none
 * @retval  void
 */
void single_tone_exit_timeout_cb(TimerHandle_t p_timer) {
    WDT_SystemReset(RESET_ALL_EXCEPT_AON, SINGLE_TONE_TIMEOUT_RESET);
}
#endif
#endif

#if (MP_TEST_SINGLE_TONE_MODE == HCI_LAYER_SINGLE_TONE_INTERFACE)
/******************************************************************
 * @brief   single tone module init
 * @param   none
 * @return  none
 * @retval  void
 */
void single_tone_init(void) {
    LOG_INF("Single Tone Init");

#if EXIT_SINGLE_TONE_TEST_WHEN_TIMEOUT
    if (true == os_timer_create(&single_tone_exit_timer, "single_tone_exit_timer", 1,
                                EXIT_SINGLE_TONE_TIME, false, single_tone_exit_timeout_cb)) {
        os_timer_start(&single_tone_exit_timer);
    }
#endif
    bool ret = os_task_create(&single_tone_task_handle, "single_tone", single_tone_task, 0, 512, 1);
    DBG_DIRECT("create single tone,ret = %d", ret);
}
#endif

int mp_test_command_handler(uint8_t *data, int len) {
    /* app can define some behaviour according special command */
}

void vhci_init(void) {
    bool ret = os_task_create(&rf_test_task_handle, "rf_test", rf_test_task, 0, 512, 1);
}
