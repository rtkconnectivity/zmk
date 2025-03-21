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
#include "trace.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/board.h>
#include <zmk/ir/ir_protocol_nec.h>

#define DBG_ON 0

#if SUPPORT_IR_TX_FEATURE
/*============================================================================*
 *                              Macros
 *============================================================================*/
#define abd_f(a,b) (a>b) ? (a-b):(b-a) ;

/*============================================================================*
 *                              Variables
 *============================================================================*/
const T_IR_NEC_SPEC NEC_SPEC =
{
    2,                                                 //  header_len;
    {PULSE_HIGH | 9000, PULSE_LOW | (4500 - IR_SAPCE_TIME_OFFSET)  },    //uint32_t HeaderContext[NEC_MAX_HEADDER_LEN];
    {PULSE_HIGH | 560, PULSE_LOW | (560 - IR_SAPCE_TIME_OFFSET)    },    //Log0[NEC_MAX_LOG_WAVFORM_SIZE];
    {PULSE_HIGH | 560, PULSE_LOW | (1690 - IR_SAPCE_TIME_OFFSET)   },    //Log1[NEC_MAX_LOG_WAVFORM_SIZE];
    PULSE_HIGH | 560,
    560,
    4
};

const T_IR_NEC_REPEAT_CODE_SPEC NEC_REPEAT_CODE_SPEC =
{
    3,                                                 /* length of repeat code */
    {PULSE_HIGH | 9000, PULSE_LOW | 2250, PULSE_HIGH | 560},  /* Buffer of repeat code: high 9ms, low 2.25ms, high 560us */
};

T_IR_NEC_BUF nec_data_buf;

/*============================================================================*
 *                              Local Functions
 *============================================================================*/
/******************************************************************
 * @brief   hl_time2_tx_buf_countng
 * @param   uint32_t log_context
 * @param   float base_time
 * @return  result
 * @retval  uint32_t
 */
static uint32_t hl_time2_tx_buf_count(uint32_t log_context, float base_time)
{
    return ((log_context & 0x80000000) | (uint32_t)((log_context & 0x7FFFFFFF) / base_time));
}

/******************************************************************
 * @brief   bit_n
 * @param   uint32_t a
 * @param   uint32_t b
 * @return  result
 * @retval  uint8_t
 */
static uint8_t bit_n(uint32_t a, uint32_t b)
{
    return ((a >> b) & 0x01);
}

/******************************************************************
 * @brief   ir_protocol_nec_command_set_tx_buf
 * @param   T_IR_NEC_BUF *p_ir_data_buf
 * @param   T_IR_NEC_SPEC p_spec
 * @return  result
 * @retval  T_IRDA_RET
 */
static T_IRDA_RET ir_protocol_nec_command_set_tx_buf(T_IR_NEC_BUF *p_ir_data_buf,
                                                     T_IR_NEC_SPEC *p_spec)
{
    int i = 0, n = 0;
    float  base_time = 0;
    uint16_t  buf_len = 0;

    uint32_t  Log1[NEC_MAX_LOG_WAVFORM_SIZE];
    uint32_t  Log0[NEC_MAX_LOG_WAVFORM_SIZE];
    uint8_t   Code = 0;

    if (p_ir_data_buf->carrier_frequency_hz == 0)
    {
        return IRDA_ERROR;
    }

    base_time = 1000000 / p_ir_data_buf->carrier_frequency_hz;

    for (i = 0; i < NEC_MAX_LOG_WAVFORM_SIZE; i++)
    {
        Log1[i] = hl_time2_tx_buf_count(p_spec->log1_context[i],
                                        base_time); // (p_spec->log1_context[i] &0x80000000 )| ((p_spec->Log1_Time[i]&0x7FFFFFFF) / base_time);
        Log0[i] = hl_time2_tx_buf_count(p_spec->log0_context[i],
                                        base_time); //(p_spec->log0_context[i] &0x80000000 )| ((p_spec->Log0_Time[i]&0x7FFFFFFF) / base_time);

    }

    /* header */
    for (i = 0; i < p_spec->header_len; i++)
    {
        p_ir_data_buf->p_buf[i] =  hl_time2_tx_buf_count(p_spec->header_context[i], base_time);
    }

    buf_len = p_spec->header_len;
    for (i = 0; i < p_ir_data_buf->code_len; i++)
    {
        Code = p_ir_data_buf->code[i];
        for (n = 0; n < 8; n++)
        {
            /* Change encoding bit order */
            if (bit_n(Code, n) == 0x01) //if (bit_n(Code,7-n)== 0x01)
            {
                /* log 1 */
                p_ir_data_buf->p_buf[buf_len] = Log1[0];
                p_ir_data_buf->p_buf[buf_len + 1] = Log1[1];
            }
            else
            {
                /* log 0 */
                p_ir_data_buf->p_buf[buf_len] = Log0[0];
                p_ir_data_buf->p_buf[buf_len + 1] = Log0[1];
            }
            buf_len += NEC_MAX_LOG_WAVFORM_SIZE;
        }
    }

    /* Stop */
    if (p_spec->stop_context != 0x0000)
    {
        p_ir_data_buf->p_buf[buf_len] = hl_time2_tx_buf_count(p_spec->stop_context, base_time);
        buf_len += 1;
    }
    p_ir_data_buf->buf_len = buf_len;

    return IRDA_SUCCEED;
}

#if IR_NEC_DECODE
/******************************************************************
 * @brief   hl_wave
 * @param   a
 * @return  result
 * @retval  uint32_t
 */
static uint32_t hl_wave(uint32_t a)
{
    return ((a & 0x80000000) >> 15);
}

/******************************************************************
 * @brief   tx_buf_count2_time
 * @param   uint32_t a
 * @param   float base_time
 * @return  result
 * @retval  float
 */
static float tx_buf_count2_time(uint32_t a, float base_time)
{
    return ((a & 0x7FFFFFFF) * base_time);
}

/******************************************************************
 * @brief   ir_protocol_invse_pulse1
 * @param   buf
 * @param   length
 * @return  result
 * @retval  T_IRDA_RET
 */
static T_IRDA_RET ir_protocol_invse_pulse1(uint32_t *buf, uint16_t length)
{
    uint16_t i = 0;

    for (i = 0; i < length; i++)
    {
        if (buf[i] > PULSE_HIGH)
        {
            buf[i] = buf[i] & (~PULSE_HIGH);
        }
        else
        {
            buf[i] = buf[i] | PULSE_HIGH;
        }
    }
    return IRDA_SUCCEED;
}

/******************************************************************
 * @brief   ir_protocol_rx_buf_log_data_check
 * @param   uint32_t *buf
 * @param   uint32_t *p_log
 * @param   uint16_t log_wav_form_len
 * @param   uint32_t base_time
 * @param   uint32_t dif_base
 * @param   uint32_t dif_base_time
 * @return  result
 * @retval  T_IRDA_RET
 */
static T_IRDA_RET ir_protocol_rx_buf_log_data_check(uint32_t *buf,
                                                    uint32_t *p_log,
                                                    uint16_t log_wav_form_len,
                                                    float base_time,
                                                    float dif_base,
                                                    float dif_base_time)
{
    float     time1 = 0, time2 = 0;
    uint16_t  i = 0;
    uint16_t  buf_value;
    float     dif = 0;
    float     temp = 0;
    for (i = 0; i < log_wav_form_len; i++)
    {
        buf_value = buf[i];

        /* check H_L */
        if (hl_wave(buf_value) != hl_wave(p_log[i]))
        {
            return IRDA_ERROR;
        }
        /* Check Time */
        time1 =  tx_buf_count2_time(buf_value, base_time);
        time2 =  p_log[i] & 0x7FFFFFFF;

        dif = (time2 / dif_base) * dif_base_time;

        temp =  abd_f(time1, time2);
        if (temp > dif)
        {
            return IRDA_ERROR;
        }
    }

    return IRDA_SUCCEED;
}

/******************************************************************
 * @brief   ir_protocol_rx_buf_log_data_check
 * @param   T_IR_NEC_BUF *p_ir_data_buf
 * @param   T_IR_NEC_SPEC *p_spec
 * @return  result
 * @retval  T_IRDA_RET
 */
static T_IRDA_RET ir_protocol_nec_set_rx_buf(T_IR_NEC_BUF *p_ir_data_buf, T_IR_NEC_SPEC *p_spec)
{
    float     base_time = 0;
    float     dif_base_time = 0;
    float     dif = 0;
    uint32_t  buf_count = 0;
    float     temp = 0;
    float     time1, time2;
    uint8_t   bit_num = 0;
    uint8_t   byte_num = 0;
    uint8_t   data;
    uint8_t   bit_type = 0;
    uint16_t  buf_len = 0;

    if (p_ir_data_buf->carrier_frequency_hz == 0)
    {
        return IRDA_ERROR;
    }

    if (ir_protocol_invse_pulse1(p_ir_data_buf->p_buf, p_ir_data_buf->buf_len) != 0)
    {
        return IRDA_ERROR;
    }

    base_time = 1000000 / p_ir_data_buf->carrier_frequency_hz;

    if (p_spec->dif_divisor == 0)
    {
        p_spec->dif_divisor = 1;
    }

    dif_base_time = p_spec->dif_base / p_spec->dif_divisor;

    if (p_spec->stop_context != 0x0000)
    {
        buf_len = p_ir_data_buf->buf_len -= 2;
    }
    else
    {
        buf_len = p_ir_data_buf->buf_len - 1;
    }

    /* Check header */
    while (buf_count < p_spec->header_len)
    {
        if (hl_wave(p_ir_data_buf->p_buf[buf_count]) != hl_wave(p_ir_data_buf->p_buf[buf_count]))
        {
            return IRDA_HEADER_ERROR;
        }

        time1 =  tx_buf_count2_time(p_ir_data_buf->p_buf[buf_count], base_time);
        time2 =  p_spec->header_context[buf_count] & 0x7FFFFFFF;

        dif = time2 / (p_spec->dif_base) * dif_base_time;

        temp =  abd_f(time1, time2);

        if (temp > dif)
        {
            return IRDA_HEADER_ERROR;
        }
        buf_count++;
    }

    bit_num = 0;
    byte_num = 0;
    data = 0;

    while (buf_count < buf_len)
    {
        /* check log 0 */
        if (ir_protocol_rx_buf_log_data_check(&p_ir_data_buf->p_buf[buf_count],
                                              p_spec->log0_context,
                                              NEC_MAX_LOG_WAVFORM_SIZE,
                                              base_time,
                                              p_spec->dif_base,
                                              dif_base_time) == 0)
        {
            bit_type = LOG_LOW;
            buf_count += NEC_MAX_LOG_WAVFORM_SIZE;
        }
        else if (ir_protocol_rx_buf_log_data_check(&p_ir_data_buf->p_buf[buf_count],
                                                   p_spec->log1_context,
                                                   NEC_MAX_LOG_WAVFORM_SIZE,
                                                   base_time,
                                                   p_spec->dif_base,
                                                   dif_base_time) == 0)
        {
            bit_type = LOG_HIGH;
            buf_count += NEC_MAX_LOG_WAVFORM_SIZE;
        }
        else
        {
            return IRDA_DATA_ERROR;
        }
        /* Change data's bit order */
        data |= (bit_type << bit_num);
        bit_num++;
#ifdef DBG_ON
        LOG_DBG("(%d/%d) data = %x --> %d \n", bit_num, byte_num, data, bit_type);
#endif
        if (bit_num >= 8)
        {
            p_ir_data_buf->code[byte_num] = data;
            byte_num++;
            bit_num = 0;
            data = 0;
            p_ir_data_buf->code_len = byte_num;

        }
    }
    return IRDA_SUCCEED;
}
#endif /*endif @IR_NEC_DECODE*/

/*============================================================================*
 *                              Global Functions
 *============================================================================*/

/******************************************************************
 * @brief   encode ir command data with NEC protocol
 * @param   uint8_t ir_key_command
 * @param   T_IR_SEND_PARA *p_ir_send_parameters
 * @return  result
 * @retval  T_IRDA_RET
 */
uint32_t *test = 0;
T_IRDA_RET ir_protocol_nec_command_encode(uint8_t ir_key_command,
                                          T_IR_SEND_PARA *p_ir_send_parameters)
{
    T_IRDA_RET ret = IRDA_SUCCEED;
    // DBG_DIRECT("before p_ir_send_par/ameters->carrier_frequency_hz %d, duty_cycle %d,command_time_ms %d", 
                // p_ir_send_parameters->carrier_frequency_hz, p_ir_send_parameters->duty_cycle, p_ir_send_parameters->command_time_ms);
    p_ir_send_parameters->carrier_frequency_hz = NEC_SEND_FREQUENCY;
    p_ir_send_parameters->duty_cycle = IR_DUTY_CYCLE;
    p_ir_send_parameters->command_time_ms = NEC_IR_COMMAND_PERIOD;

    // DBG_DIRECT("p_ir_send_parameters,command_time_ms %d", 
                //  p_ir_send_parameters->command_time_ms);
    // LOG_ERR("after p_ir_send_parameters->carrier_frequency_hz %d, duty_cycle %d,command_time_ms %d", 
    //             p_ir_send_parameters->carrier_frequency_hz, p_ir_send_parameters->duty_cycle, p_ir_send_parameters->command_time_ms);
    // DBG_DIRECT("ir para carrie freq = %d, test value %d, addr 0x%x", *(float *)0x20a6d4, *test, &test);
    /* set p_ir_send_buf and send_buf_len */

    uint8_t nec_address = NEC_ADDRESS;

    nec_data_buf.carrier_frequency_hz = NEC_SEND_FREQUENCY; /* 38kHz */
    nec_data_buf.code_len = NEC_CODE_LEN; /* 4 */
    nec_data_buf.code[0] = nec_address;
    nec_data_buf.code[1] = ~nec_address;
    nec_data_buf.code[2] = ir_key_command;
    nec_data_buf.code[3] = ~ir_key_command;
    nec_data_buf.p_buf = p_ir_send_parameters->ir_send_buf;

    ret = ir_protocol_nec_command_set_tx_buf(&nec_data_buf, (T_IR_NEC_SPEC *)&NEC_SPEC);
    p_ir_send_parameters->send_buf_len = nec_data_buf.buf_len;
#if DBG_ON
    LOG_DBG("=========== NEC Tx buf===========================\n");
    LOG_DBG("address = 0x%x command = 0x%x \n", nec_address, ir_key_command);
    LOG_DBG("buf_len= 0x%x\n", nec_data_buf.buf_len);

    for (uint8_t i = 0; i < nec_data_buf.buf_len; i++)
    {
        if (i % 8 == 0)
        {
            LOG_DBG("\n");
        }
        LOG_DBG("%x,", nec_data_buf.p_buf[i]);
    }
    LOG_DBG("\n");
    LOG_DBG("============= ret = %d===============\n", ret);
#endif
    return ret;
}

/******************************************************************
 * @brief   encode ir repeat code data with NEC protocol
 * @param   T_IR_SEND_PARA *p_ir_send_parameters
 * @return  result
 * @retval  T_IRDA_RET
 */
T_IRDA_RET ir_protocol_nec_repeat_code_encode(T_IR_SEND_PARA *p_ir_send_parameters)
{
    T_IRDA_RET ret = IRDA_SUCCEED;

    if (p_ir_send_parameters->carrier_frequency_hz < 5000 ||
        p_ir_send_parameters->carrier_frequency_hz > 2000000)
    {
        DBG_DIRECT("ir_protocol_nec_repeat_code_encode check fail:carrier_frequency_hz %d", p_ir_send_parameters->carrier_frequency_hz);
        ret = IRDA_ERROR;
        return ret;
    }
    DBG_DIRECT("p_ir_send_parameters->carrier_frequency_hz %f", p_ir_send_parameters->carrier_frequency_hz);
    uint16_t index = 0;
    p_ir_send_parameters->repeat_code_time_ms = NEC_IR_REPEAT_CODE_PERIOD;
    p_ir_send_parameters->repeat_buf_len = NEC_REPEAT_CODE_SPEC.repeat_code_len;
    /* Encode repeat code */
    for (index = 0; index < p_ir_send_parameters->repeat_buf_len; index++)
    {
        p_ir_send_parameters->ir_repeat_code_buf[index] =  ir_send_convert_to_carrier_cycle(
                                                               NEC_REPEAT_CODE_SPEC.repeat_code_buf[index],
                                                               p_ir_send_parameters->carrier_frequency_hz);
    }

    return ret;
}

#if IR_NEC_DECODE
/******************************************************************
 * @brief   decode ir data with NEC protocol
 * @param   uint8_t *address
 * @param   uint8_t *command
 * @param   T_IR_NEC_BUF *p_ir_data
 * @return  result
 * @retval  T_IRDA_RET
 */
T_IRDA_RET ir_protocol_nec_decode(uint8_t *address, uint8_t *command, T_IR_NEC_BUF *p_ir_data)
{
    T_IRDA_RET ret = IRDA_SUCCEED;
    p_ir_data->code_len = 0;
    p_ir_data->code[0] = 0;
    p_ir_data->code[1] = 0;
    p_ir_data->code[2] = 0;
    p_ir_data->code[3] = 0;

    ret = ir_protocol_nec_set_rx_buf(p_ir_data, (T_IR_NEC_SPEC *)&NEC_SPEC);

    *address = p_ir_data->code[0];
    *command =  p_ir_data->code[2];

    LOG_DBG("=========== NEC Rx Code===========================\n");
    LOG_DBG("address = 0x%x command = 0x%x \n", *address, *command);
    LOG_DBG("\n");
    return ret;
}
#endif

#endif