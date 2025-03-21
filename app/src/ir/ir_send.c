/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/drivers/ir.h>
#include <zephyr/logging/log.h>
#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <zmk/board.h>
#include <zmk/ir/ir_send.h>
#include <ZMK/ir/ir_learn.h>
#include <zmk/ir/ir_protocol_nec.h>
#include "trace.h"
#include "rtl876x_pinmux.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if SUPPORT_IR_TX_FEATURE

/* BLE HID code table definition */
const T_KEY_CODE_DEF KEY_CODE_TABLE[KEY_CODE_TABLE_SIZE] =
{
    /* key_type,            ir_key_code,     hid usage page,         hid_usage_id */
    {KEY_TYPE_NONE,         0x00,           HID_USAGE_UNDEFINED,     0x00},  /* VK_NONE */
    {KEY_TYPE_BLE_OR_IR,    0x18,           HID_USAGE_KEY,           0x66},  /* VK_POWER */
    {KEY_TYPE_BLE_OR_IR,    0x08,           HID_USAGE_KEY,           0x4B},  /* VK_PAGE_UP */
    {KEY_TYPE_BLE_OR_IR,    0x09,           HID_USAGE_KEY,           0x4E},  /* VK_PAGE_DOWN */
    {KEY_TYPE_BLE_OR_IR,    0x56,           HID_USAGE_KEY,           0x76},  /* VK_MENU */
    {KEY_TYPE_BLE_OR_IR,    0x14,           HID_USAGE_KEY,           0x4A},  /* VK_HOME */
    {KEY_TYPE_BLE_ONLY,     0x3E,           HID_USAGE_KEY,           0x3E},  /* VK_VOICE */
    {KEY_TYPE_BLE_OR_IR,    0x4C,           HID_USAGE_KEY,           0x28},  /* VK_ENTER */
    {KEY_TYPE_BLE_OR_IR,    0x57,           HID_USAGE_KEY,           0x29},  /* VK_EXIT */
    {KEY_TYPE_BLE_OR_IR,    0x0C,           HID_USAGE_KEY,           0x50},  /* VK_LEFT */
    {KEY_TYPE_BLE_OR_IR,    0x0E,           HID_USAGE_KEY,           0x4F},  /* VK_RIGHT */
    {KEY_TYPE_BLE_OR_IR,    0x4D,           HID_USAGE_KEY,           0x52},  /* VK_UP */
    {KEY_TYPE_BLE_OR_IR,    0x48,           HID_USAGE_KEY,           0x51},  /* VK_DOWN */
    {KEY_TYPE_NONE,         0x00,           HID_USAGE_UNDEFINED,     0x00},  /* VK_MOUSE_EN */
    {KEY_TYPE_BLE_OR_IR,    0x7F,           HID_USAGE_KEY,           0x7F},  /* VK_VOLUME_MUTE */
    {KEY_TYPE_BLE_OR_IR,    0x49,           HID_USAGE_KEY,           0x80},  /* VK_VOLUME_UP */
    {KEY_TYPE_BLE_OR_IR,    0x4B,           HID_USAGE_KEY,           0x81},  /* VK_VOLUME_DOWN */
    {KEY_TYPE_BLE_ONLY,     0x3F,           HID_USAGE_KEY,           0x3F},  /* VK_VOICE_STOP */
    {KEY_TYPE_IR_ONLY,      0x01,           HID_USAGE_KEY,           0x00},  /* VK_TV_POWER */
    {KEY_TYPE_IR_ONLY,      0x5A,           HID_USAGE_KEY,           0x00},  /* VK_TV_SIGNAL */
#if FEATURE_SUPPORT_MULTIMEDIA_KEYBOARD       
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xb5},   /* MM_ScanNext */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xb6},   /* MM_ScanPrevious */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xb7},   /* MM_Stop */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xcd},   /* MM_Play_Pause */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xe2},   /* MM_Mute */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xe5},   /* MM_BassBoost */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xe7},   /* MM_Loudness */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xe9},   /* MM_VolumeIncrement */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xea},   /* MM_VolumeDecrement */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0152}, /* MM_BassIncrement */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0153}, /* MM_BassDecrement */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0154}, /* MM_TrebleIncrement */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0155}, /* MM_TrebleDecrement */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0183}, /* MM_AL_ConsumerControl */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x018a}, /* MM_AL_EmailReader */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0192}, /* MM_AL_Calculator */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0194}, /* MM_AL_LocalMachineBrowser */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0221}, /* MM_AC_Search */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0223}, /* MM_AC_Home */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0224}, /* MM_AC_Back */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0225}, /* MM_AC_Forward */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0226}, /* MM_AC_Stop */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0227}, /* MM_AC_Refresh */
    {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x022a}, /* MM_AC_Bookmarks */
#endif
};
/*============================================================================*
 *                          Local Variables
 *============================================================================*/
static T_IR_SEND_STRUCT ir_send_struct;
static T_IR_SEND_KEY_STATE ir_send_key_state = IR_SEND_KEY_RELEASE;
T_IR_SEND_PARA ir_send_parameters = {0};
uint32_t tx_len = 100;
uint32_t tx_sent;
const struct device *const ir_dev = DEVICE_DT_GET(DT_NODELABEL(ir));

struct ir_send_msg_processor {
    struct k_work work;
} ir_send_msg_processor;

struct ir_msg {
    uint32_t state;
    uint32_t type;
};

/******************************************************************
 * @brief    convert time and frequency to carrier cycle.
 * @param    time - time of waveform.
 * @param    carrier_cycle - cycle of carrier.
 * @return   vaule of data whose unit is cycle of carrier.
 */
uint32_t ir_send_convert_to_carrier_cycle(uint32_t time, uint32_t freq)
{
    return ((time & PULSE_HIGH) | ((time & IR_DATA_MSK) * freq / 1000000));
}

void ir_send_repeat_code_timer_cb(struct k_timer *_timer);

K_TIMER_DEFINE(ir_send_repeat_code_timer, ir_send_repeat_code_timer_cb, NULL);
K_MSGQ_DEFINE(ir_msgq, sizeof(struct ir_msg), IR_EVENT_QUEUE_SIZE, 4);


void ir_send_repeat_code_timer_cb(struct k_timer *_timer)
{
	struct ir_msg ev = {
        .state = ir_send_struct.ir_send_state,
        .type = IO_MSG_TYPE_IR_START_SEND_REPEAT_CODE
	};

    k_msgq_put(&ir_msgq, &ev, K_NO_WAIT);
    LOG_DBG("ir_send_repeat_code_timer_cb");
    k_work_submit(&ir_send_msg_processor.work);
}

void ir_send_callback(const struct device *dev, struct ir_event *evt, void *user_data)
{
    Pad_Config(P3_0, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_DOWN, PAD_OUT_ENABLE,
                   PAD_OUT_LOW);
    if(ir_send_struct.ir_send_state == IR_SEND_CAMMAND) {
        tx_sent += evt->data.tx.len;
        // LOG_DBG("ir send cb: cur_tx_send %d, total len %d",tx_sent, ir_send_struct.p_ir_send_data->send_buf_len);
        // LOG_DBG("tx buf 0x%x, CUR State %d",ir_send_struct.p_ir_send_data->ir_send_buf[tx_sent],ir_send_struct.ir_send_state);
        if (tx_sent < ir_send_struct.p_ir_send_data->send_buf_len) {
            Pad_Config(P3_0, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_DOWN, PAD_OUT_ENABLE,
                    PAD_OUT_HIGH);
            ir_tx(ir_dev, &(ir_send_struct.p_ir_send_data->ir_send_buf[tx_sent]), ir_send_struct.p_ir_send_data->send_buf_len - tx_sent);
        } else {
            tx_sent = 0;
            ir_send_struct.ir_send_state = IR_SEND_CAMMAND_COMPLETE;
        }
    } else if(ir_send_struct.ir_send_state == IR_SEND_REPEAT_CODE) {
        tx_sent += evt->data.tx.len;
        // LOG_DBG("ir send cb: cur_tx_send %d, total len %d",tx_sent, ir_send_struct.p_ir_send_data->repeat_buf_len);
        // LOG_DBG("tx buf 0x%x, CUR State %d",ir_send_struct.p_ir_send_data->ir_send_buf[tx_sent],ir_send_struct.ir_send_state);
        if (tx_sent < ir_send_struct.p_ir_send_data->repeat_buf_len) {
            Pad_Config(P3_0, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_DOWN, PAD_OUT_ENABLE,
                    PAD_OUT_HIGH);
            ir_tx(ir_dev, &(ir_send_struct.p_ir_send_data->ir_repeat_code_buf[tx_sent]), ir_send_struct.p_ir_send_data->repeat_buf_len - tx_sent);
        } else {
            tx_sent = 0;
            ir_send_struct.ir_send_state = IR_SEND_REPEAT_CODE_COMPLETE;
        }
    }

    // LOG_DBG("[%s] dev:%s evt:%d tx_len:%d\n", __func__, dev->name, evt->type, evt->data.tx.len);
	struct ir_msg ev = {
        .state = ir_send_struct.ir_send_state,
        .type = IO_MSG_TYPE_IR_SEND_COMPLETE
	};

    k_msgq_put(&ir_msgq, &ev, K_NO_WAIT);
    k_work_submit(&ir_send_msg_processor.work);
}

/******************************************************************
* @brief   encode ir send command data
* @param   T_IR_PROTOCOL protocol
* @param   uint8_t ir_key_command
* @param   T_IR_SEND_PARA *p_ir_send_parameters
* @return  result
* @retval  T_IRDA_RET
*/
static T_IRDA_RET ir_send_command_encode(T_IR_PROTOCOL ir_protocol, uint8_t ir_key_command,
                                         T_IR_SEND_PARA *p_ir_send_parameters)
{
    T_IRDA_RET ret = IRDA_SUCCEED;

    switch (ir_protocol)
    {
    case NEC_PROTOCOL:
            DBG_DIRECT("[ir_protocol_nec_command_encode]: p_ir_send_parameters addr 0x%x",p_ir_send_parameters);
        ret = ir_protocol_nec_command_encode(ir_key_command, p_ir_send_parameters);
        break;
    default:
        break;
    }

    return ret;
}

/******************************************************************
* @brief   encode ir send repeat code data
* @param   T_IR_PROTOCOL protocol
* @param   uint8_t ir_key_command
* @param   T_IR_SEND_PARA *p_ir_send_parameters
* @return  result
* @retval  T_IRDA_RET
*/
static T_IRDA_RET ir_send_repeat_code_encode(T_IR_PROTOCOL ir_protocol, uint8_t ir_key_command,
                                             T_IR_SEND_PARA *p_ir_send_parameters)
{
    T_IRDA_RET ret = IRDA_SUCCEED;

    switch (ir_protocol)
    {
    case NEC_PROTOCOL:
        ret = ir_protocol_nec_repeat_code_encode(p_ir_send_parameters);
        break;
    default:
        break;
    }

    return ret;
}

void ir_send_msg_proc(struct k_work *item)
{
    LOG_DBG("ir_send_get_current_state is %d",ir_send_get_current_state());

    struct ir_msg ev;
    while (k_msgq_get(&ir_msgq, &ev, K_NO_WAIT) == 0) {
		if (ev.type == IO_MSG_TYPE_IR_START_SEND_REPEAT_CODE)
		{
			if (ir_send_get_current_state() == IR_SEND_CAMMAND_COMPLETE ||
				ir_send_get_current_state() == IR_SEND_REPEAT_CODE_COMPLETE)
			{
                Pad_Config(P3_0, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_DOWN, PAD_OUT_ENABLE,
                   PAD_OUT_HIGH);
				if (false == ir_send_repeat_code_start())
				{
                    LOG_DBG("ir_send_repeat_code_start RETURN FALSE");
					ir_send_exit();
				}
			}
			else
			{
                LOG_DBG("STATE err");
				ir_send_exit();
			}
		}
		else if (ev.type == IO_MSG_TYPE_IR_SEND_COMPLETE)
		{
			if (ir_send_key_state == IR_SEND_KEY_RELEASE)
			{
                LOG_DBG("ir send key release");
				ir_send_exit();
			}
		}	
	}


}

bool ir_send_is_working(void)
{
    if (ir_send_struct.ir_send_state == IR_SEND_IDLE)
    {
        return false;
    }
    return true;
}

T_IR_SEND_STATE ir_send_get_current_state(void)
{
	return ir_send_struct.ir_send_state;
}

bool ir_send_command_start(void)
{
    if (ir_send_struct.p_ir_send_data->carrier_frequency_hz < 5000 ||
        ir_send_struct.p_ir_send_data->carrier_frequency_hz > 2000000)
    {
        LOG_ERR("[IR] ir send command start fail, carrier frequecy <5KHz or >2MHz.");
        return false;
    }
    

    // ir_send_struct.p_ir_send_data->ir_send_buf[ir_send_struct.p_ir_send_data->send_buf_len - 1] |=
    //     BIT(30);

    /* enable IR TX */
    LOG_DBG("[IR] ir send start command.len=%d, repeat code time %d",ir_send_struct.p_ir_send_data->send_buf_len,ir_send_parameters.command_time_ms);
 	ir_set_freq(ir_dev, ir_send_struct.p_ir_send_data->carrier_frequency_hz, ir_send_struct.p_ir_send_data->duty_cycle);
    ir_tx_enable(ir_dev, ir_send_callback, NULL);
 	ir_tx(ir_dev, (uint32_t)ir_send_struct.p_ir_send_data->ir_send_buf, ir_send_struct.p_ir_send_data->send_buf_len);
        Pad_Config(P3_0, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_DOWN, PAD_OUT_ENABLE,
                   PAD_OUT_HIGH);
    /*enable ir_send_repeat_code_timer */
	k_timer_start(&ir_send_repeat_code_timer, K_MSEC(ir_send_parameters.command_time_ms), K_MSEC(ir_send_parameters.command_time_ms));

    ir_send_struct.ir_send_state = IR_SEND_CAMMAND;

    return true;
}

bool ir_send_repeat_code_start(void)
{
    if (ir_send_struct.p_ir_send_data->carrier_frequency_hz < 5000 ||
        ir_send_struct.p_ir_send_data->carrier_frequency_hz > 2000000)
    {
        LOG_ERR("[IR] ERR: ir send carrier frequency <5KHz or >2MHz.");
        return false;
    }

    if (ir_send_struct.p_ir_send_data->repeat_buf_len == 0)
    {
        LOG_ERR("[IR] ERR: ir send repeat buffer length is 0.");
        return false;
    }

    // LOG_DBG("[IR] ir send start repeat code.");

    // ir_send_struct.p_ir_send_data->ir_repeat_code_buf[ir_send_struct.p_ir_send_data->repeat_buf_len - 1]
    // |= BIT(30);
    // LOG_DBG("send repeat code 1 %x, 2 %x, 3 %x",ir_send_struct.p_ir_send_data->ir_repeat_code_buf[0],ir_send_struct.p_ir_send_data->ir_repeat_code_buf[1],
    //             ir_send_struct.p_ir_send_data->ir_repeat_code_buf[2]);
    /* enable IR TX */
 	// ir_set_freq(ir_dev, 38000, 3);
 	ir_tx_enable(ir_dev, ir_send_callback, NULL);
 	ir_tx(ir_dev, (uint32_t)ir_send_struct.p_ir_send_data->ir_repeat_code_buf, ir_send_struct.p_ir_send_data->repeat_buf_len);


    /* enable ir_send_repeat_code_timer  */
	k_timer_start(&ir_send_repeat_code_timer, K_MSEC(ir_send_struct.p_ir_send_data->repeat_code_time_ms), K_MSEC(ir_send_struct.p_ir_send_data->repeat_code_time_ms));

    ir_send_struct.ir_send_state = IR_SEND_REPEAT_CODE;

    return true;
}

void ir_send_exit(void)
{
	LOG_DBG("[IR] ir send exit.");
	k_timer_stop(&ir_send_repeat_code_timer);
    ir_send_struct.ir_send_state = IR_SEND_IDLE;
}

/******************************************************************
 * @brief    Get IR key code by key index
 * @param    key_index - key index
 * @return   uint8_t - ir key code
 * @retval   void
 */
uint8_t key_handle_get_ir_key_code_by_index(uint32_t key_index)
{
    return KEY_CODE_TABLE[key_index].ir_key_code;
}

/******************************************************************
 * @brief    Get IR key code by key usage id
 * @param    uint32_t - usage id
 * @return   uint8_t - ir key code
 * @retval   void
 */
uint8_t key_handle_get_ir_key_code_by_usage_id(uint32_t usage_id)
{
    for(uint8_t i = 0; i < KEY_CODE_TABLE_SIZE; i++)
    {
        DBG_DIRECT("Table hid usage id: %d", KEY_CODE_TABLE[i].hid_usage_id);
        if (KEY_CODE_TABLE[i].hid_usage_id == usage_id)
        {
            return KEY_CODE_TABLE[i].ir_key_code;
        }
    }
    DBG_DIRECT("not find ir code by usage id");
    return 0xff;
}
#include "trace.h"
/******************************************************************
 * @brief   Application code for IR send key press handle.
 * @param   uint32_t key_index
 * @return  none
 * @retval  void
 */
void ir_send_key_press_handle(uint32_t ir_send_key_index)
{
    DBG_DIRECT("[IR] ir send key press.key index is %d", ir_send_key_index);
    ir_send_key_state = IR_SEND_KEY_PRESS;

#if SUPPORT_IR_LEARN_FEATURE
    if (ir_learn_is_working())
    {
        LOG_DBG("[IR] IR learn is working, can't start ir send.");
        return;
    }
#endif
    memset(&ir_send_parameters, 0, sizeof(T_IR_SEND_PARA));

#if SUPPORT_IR_LEARN_FEATURE
    if (false == ir_get_learned_wave_data(ir_send_key_index, &ir_send_parameters))
    {
        uint8_t ir_code = key_handle_get_ir_key_code_by_usage_id(ir_send_key_index);
        DBG_DIRECT("SUPPORT_IR_LEARN_FEATURE: ir code is 0x%x, ir para addr 0x%x", ir_code, &ir_send_parameters);
        ir_send_command_encode(NEC_PROTOCOL, ir_code, &ir_send_parameters);
        ir_send_repeat_code_encode(NEC_PROTOCOL, ir_code, &ir_send_parameters);
    }
#else
    uint8_t ir_code = key_handle_get_ir_key_code_by_usage_id(ir_send_key_index);
    ir_send_command_encode(NEC_PROTOCOL, ir_code, &ir_send_parameters);
    ir_send_repeat_code_encode(NEC_PROTOCOL, ir_code, &ir_send_parameters);
#endif
    if (true == ir_send_module_init(&ir_send_parameters))
    {
        ir_send_command_start();
    }
}

/******************************************************************
 * @brief   Application code for IR send key release handle.
 * @param   none
 * @return  none
 * @retval  void
 */
void ir_send_key_release_handle(void)
{
    LOG_DBG("[IR] ir send key release.");
    ir_send_key_state = IR_SEND_KEY_RELEASE;

#if SUPPORT_IR_LEARN_FEATURE
    if (ir_learn_is_working())
    {
        return;
    }
#endif
    if (ir_send_is_working() == false)
    {
        return;
    }

    if (ir_send_get_current_state() != IR_SEND_CAMMAND &&
        ir_send_get_current_state() != IR_SEND_REPEAT_CODE)
    {
        ir_send_exit();
    }
}

bool ir_send_module_init(T_IR_SEND_PARA *p_ir_send_para)
{
    if (p_ir_send_para == NULL) {
        LOG_ERR("[IR] ir send module init fail, p_ir_send_para is NULL.");
        return false;
    }
    ir_send_struct.p_ir_send_data = p_ir_send_para;
    if (ir_send_struct.p_ir_send_data->carrier_frequency_hz < 5000 ||
        ir_send_struct.p_ir_send_data->carrier_frequency_hz > 2000000) {
        LOG_ERR("[IR] ir send module init fail, carrier frequecy <5KHz or >2MHz.");
        return false;
    }
    if (ir_send_struct.p_ir_send_data->duty_cycle <= 0) {
        LOG_ERR("[IR] ir send module init fail, duty_cycle is invalid.");
        return false;
    }
	k_work_init(&ir_send_msg_processor.work, ir_send_msg_proc);

 	return true;
}

#endif