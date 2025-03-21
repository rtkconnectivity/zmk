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
#include <zmk/board.h>
#include <zmk/ir/ir_learn.h>
#include <zmk/ir/ir_send.h>
#include <rtl876x_ir.h>


LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if (SUPPORT_IR_TX_FEATURE && SUPPORT_IR_LEARN_FEATURE)
/*============================================================================*
 *                          Local Variables
 *============================================================================*/

struct ir_learn_msg_processor {
    struct k_work work;
} ir_learn_msg_processor;

struct ir_learn_msg {
    uint32_t state;
    uint32_t type;
};

K_MSGQ_DEFINE(ir_learn_msgq, sizeof(struct ir_learn_msg), IR_EVENT_QUEUE_SIZE, 4);

static T_IR_LEARN_STRUCT ir_learn_struct;
static T_IR_LEARN_KEY_STATE ir_learn_key_state = IR_LEARN_KEY_RELEASE;
static T_IR_LEARN_PARA ir_learn_data;
static T_IR_LEARN_STORAGE_INFO ir_learn_storage_info;
static T_IR_LEARN_KEY_INFO ir_learn_table[IR_LEARN_MAX_KEY_NUM] =
{
    {0, 0x12},
    {1, 0x13},
    {2, 0x01},
};

/*============================================================================*
 *                              Functions Declaration
 *============================================================================*/
static void ir_learn_init_driver(void);

static void ir_learn_get_rx_fifo_data(void);
static void ir_learn_handler(void);
#if IR_LEARN_DUTY_CYCLE_SUPPORT
T_IR_LEARN_RESULT ir_learn_decode_duty_cycle(T_IR_LEARN_PARA *p_ir_learn_para);
#endif
T_IR_LEARN_RESULT ir_learn_decode(T_IR_LEARN_PARA *p_ir_learn_para);
static uint32_t ir_learn_get_key_offset(uint32_t ir_learn_key_index);
static bool ir_learn_set_storaged_data(T_IR_LEARN_STORAGE_INFO *p_ir_learn_storage_info,
                                       T_IR_LEARN_PARA *p_ir_learn_data);
static bool ir_learn_storage_wave_data(T_IR_LEARN_STORAGE_INFO *p_ir_learn_storage_info);

// bool ir_learn_check_dlps(void);
// void ir_learn_enter_dlps_config(void);
// void ir_learn_exit_dlps_config(void);
void ir_learn_timer_cb(struct k_timer *_timer);
K_TIMER_DEFINE(ir_learn_timer, ir_learn_timer_cb, NULL);

void ir_learn_timer_cb(struct k_timer *_timer)
{

}

/*============================================================================*
 *                              Local Functions
 *============================================================================*/
void ir_learn_callback(const struct device *dev, struct ir_event *evt, void *user_data)
{
	ir_learn_get_rx_fifo_data();
	struct ir_learn_msg ev = {
        .state = ir_learn_struct.ir_learn_state,
        .type = IO_MSG_TYPE_IR_START_SEND_REPEAT_CODE
	};

    k_msgq_put(&ir_learn_msgq, &ev, K_NO_WAIT);
    k_work_submit(&ir_learn_msg_processor.work);

}

/******************************************************************
 * @brief   write ir rx fifo data to buf
 * @param   none
 * @return  none
 * @retval  void
 */
void ir_learn_get_rx_fifo_data(void)
{
	struct ir_event_rx ir_learn_data;
	ir_rx_disable(ir_dev, &ir_learn_data);
	ir_rx_enable(ir_dev, ir_learn_callback, NULL, ir_learn_data.len, &ir_learn_data, 0);
    ir_learn_struct.p_ir_learn_data->remaining_fifo_data_len = ir_learn_data.len;

    LOG_DBG("[ir_learn_get_rx_fifo_data] ir_remain_data_len = %d",
                    ir_learn_struct.p_ir_learn_data->remaining_fifo_data_len);
    ir_learn_struct.p_ir_learn_data->in_buff_total_num++;
	memcpy(&(ir_learn_struct.p_ir_learn_data->remaining_fifo_data[0]), ir_learn_data.buf, ir_learn_data.len);
    ir_learn_struct.p_ir_learn_data->ir_learn_stop_flag = 1;
}

/******************************************************************
 * @brief   get ir learn key offset.
 * @param   ir_learn_key_index
 * @return  ir learn key offset
 * @retval  uint32_t
 */
uint32_t ir_learn_get_key_offset(uint32_t ir_learn_key_index)
{
    for (uint8_t i = 0; i < IR_LEARN_MAX_KEY_NUM; i++)
    {
        if (ir_learn_table[i].key_index == ir_learn_key_index)
        {
            return ir_learn_table[i].offset;
        }
    }
    return INVALID_OFFSET;
}

/******************************************************************
 * @brief   get ir learn wave data.
 * @param   ir_send_key_index
 * @param   p_ir_send_para
 * @return  get data successfully or not
 * @retval  true or false
 */
bool ir_get_learned_wave_data(uint32_t ir_send_key_index, T_IR_SEND_PARA *p_ir_send_para)
{
    T_IR_LEARN_STORAGE_INFO ir_learn_ftl_dada;

    uint8_t offset = ir_learn_get_key_offset(ir_send_key_index);

    if (offset != INVALID_OFFSET)
    {
        LOG_DBG("[IR] IR key index: %d, offset: %d", ir_send_key_index, offset);
        LOG_DBG("[IR] sizeof(T_IR_LEARN_STORAGE_INFO): %d", sizeof(T_IR_LEARN_STORAGE_INFO));
        // if (0 == ftl_load(&ir_learn_ftl_dada,
        //                   IR_WAVE_DATA_BASE_ADDR + offset * sizeof(T_IR_LEARN_STORAGE_INFO), sizeof(T_IR_LEARN_STORAGE_INFO)))
        // {
        //     LOG_DBG("[IR] get ftl wave data, valid flag is 0x%x", ir_learn_ftl_dada.valid_flag);
        //     LOG_DBG("[IR] get ftl wave data, offest is %d", ir_learn_ftl_dada.offset);
        //     LOG_DBG("[IR] get ftl wave data, carrier frequency is %d Hz",
        //                     (uint32_t)ir_learn_ftl_dada.frequency);
        //     LOG_DBG("[IR] get ftl wave data, duty cycle is %d percent",
        //                     (uint32_t)(100 / ir_learn_ftl_dada.duty_cycle));
        //     LOG_DBG("[IR] get ftl wave data, data len is %d", ir_learn_ftl_dada.buf_len);

        //     if (ir_learn_ftl_dada.valid_flag == VALID_FLAG &&
        //         ir_learn_ftl_dada.offset == offset &&
        //         ir_learn_ftl_dada.key_index == ir_send_key_index &&
        //         ir_learn_ftl_dada.duty_cycle > 0 &&
        //         ir_learn_ftl_dada.buf_len > 0 && ir_learn_ftl_dada.buf_len <= IR_LEARN_WAVE_MAX_LEN &&
        //         ir_learn_ftl_dada.frequency >= 5000 && ir_learn_ftl_dada.frequency <= 2000000)
        //     {
        //         p_ir_send_para->carrier_frequency_hz = ir_learn_ftl_dada.frequency;
        //         p_ir_send_para->duty_cycle = ir_learn_ftl_dada.duty_cycle;
        //         p_ir_send_para->send_buf_len = ir_learn_ftl_dada.buf_len;
        //         p_ir_send_para->repeat_buf_len = 0;

        //         memcpy(p_ir_send_para->ir_send_buf, &ir_learn_ftl_dada.ir_learn_storage_buf,
        //                IR_LEARN_WAVE_MAX_LEN * sizeof(uint32_t));

        //         LOG_DBG("[IR] get IR learn wave data success");
        //         return true;
        //     }
        // }
    }
    LOG_DBG("[IR] ERR: get IR learn wave data err");
    return false;
}

/******************************************************************
 * @brief   Application code for IR learn data process.
 * @param   msg_sub_type - IR sub type msg.
 * @return  none
 * @retval  void
 */
void ir_learn_msg_proc(struct k_work *item)
{
    T_IR_LEARN_RESULT status = IR_LEARN_OK;
    struct ir_learn_msg ev;
    while (k_msgq_get(&ir_learn_msgq, &ev, K_NO_WAIT) == 0) {
		if (ev.type == IO_MSG_TYPE_IR_LEARN_DATA)
		{
			/* Decode IR waveform data and duty cycle*/
			status = ir_learn_decode(&ir_learn_data);

			if (status != IR_LEARN_OK)
			{
				LOG_ERR("[IR] ERR: IR learn err code is %d.", status);
				ir_learn_stop();
			}

			LOG_DBG("[IR] IR learn data process ok.");
		} else if (ev.type == IO_MSG_TYPE_IR_LEARN_STOP) {
			/* pick up the last ir data */
			status = ir_learn_decode(&ir_learn_data);
			if (status == IR_LEARN_EXIT)
			{
				/* decode IR carrier freqency */
				ir_learn_freq(&ir_learn_data);
				/* convert data*/
				ir_learn_convert_data(&ir_learn_data);
				/* set ir learn data to storage*/
				if (true == ir_learn_set_storaged_data(&ir_learn_storage_info, &ir_learn_data))
				{
					/* storage ir learn valid wave */
					if (true == ir_learn_storage_wave_data(&ir_learn_storage_info))
					{
						LOG_DBG("[IR] IR learn wave data is storaged successfully.");
					}
				}
			}
			else
			{
				LOG_ERR("[IR] ERR: IR learn err code is %d.", status);
			}
			ir_learn_stop();
		}
	}
}

/*============================================================================*
 *                              Global Functions
 *============================================================================*/
#if IR_LEARN_DUTY_CYCLE_SUPPORT
/******************************************************************
 * @brief  learn a specific IR waveform duty cycle.
 * @param  p_ir_learn_para - point to IR packet struct.
 * @return the decoded status.
 * @retval T_IR_LEARN_RESULT
 */
T_IR_LEARN_RESULT ir_learn_decode_duty_cycle(T_IR_LEARN_PARA *p_ir_learn_para)
{
    uint16_t i = 0;
    uint32_t buf[2 * (IR_LEARN_DUTY_CYCLE_MAX_CYCLE_SIZE + 1)] = {0};
    uint32_t carrier_high_time = 0;
    uint32_t carrier_low_time = 0;
    uint8_t carrier_valid_count = 0;

    memcpy(buf,
           ir_learn_struct.p_ir_learn_data->gdma_buf_data[ir_learn_struct.p_ir_learn_data->out_buff_total_num %
                                                                                                              IR_RX_GDMA_LINK_LIST_NUM].buf, 2 * (IR_LEARN_DUTY_CYCLE_MAX_CYCLE_SIZE + 1) * sizeof(uint32_t));

    for (i = 1; i < IR_LEARN_DUTY_CYCLE_MAX_CYCLE_SIZE + 1; i++)
    {
        uint16_t j = 2 * i;
        if (((buf[j] & IR_DATA_MSK) < IR_LEARN_TIME_HIGHEST_VALUE) &&
            ((buf[j + 1] & IR_DATA_MSK) < IR_LEARN_TIME_HIGHEST_VALUE))
        {
            if (buf[j] & IR_CARRIER_DATA_TYPE)
            {
                carrier_high_time += buf[j] & IR_DATA_MSK;
                carrier_low_time += buf[j + 1] & IR_DATA_MSK;
            }
            else
            {
                carrier_high_time += buf[j + 1] & IR_DATA_MSK;
                carrier_low_time += buf[j] & IR_DATA_MSK;
            }
            carrier_valid_count++;
        }
    }

    if (carrier_high_time > 0 && carrier_low_time > 0)
    {
        p_ir_learn_para->duty_cycle = (float)(carrier_high_time + carrier_low_time) / carrier_high_time;
        p_ir_learn_para->carrier_time = (carrier_high_time + carrier_low_time) / carrier_valid_count;
        LOG_DBG("[ir_learn_decode_duty_cycle]: duty = %d, carrier_time = %d, count = %d",
                        (uint8_t)(p_ir_learn_para->duty_cycle), p_ir_learn_para->carrier_time, carrier_valid_count);
        return IR_LEARN_OK;
    }
    return IR_LEARN_DUTY_CYCLE_ERR_NO_VALID_DATA;
}
#endif

/******************************************************************
 * @brief  Convert IR learned waveform data to actual IR data which can be sent.
 * @param  p_ir_learn_para - point to IR packet struct.
 * @return none
 * @retval void
 */
void ir_learn_convert_data(T_IR_LEARN_PARA *p_ir_learn_para)
{
    uint16_t i = 0;
    if ((p_ir_learn_para->carrier_freq == 0) || (p_ir_learn_para->duty_cycle <= 0))
    {
        return ;
    }
    for (i = 0; i < p_ir_learn_para->ir_learn_buf_index; i++)
    {

        if (p_ir_learn_para->ir_learn_buffer[i] & IR_CARRIER_DATA_TYPE)
        {
            p_ir_learn_para->ir_learn_buffer[i] = (p_ir_learn_para->ir_learn_buffer[i] & IR_DATA_MSK) *
                                                  p_ir_learn_para->carrier_freq / IR_LEARN_FREQ;
            p_ir_learn_para->ir_learn_buffer[i] |= IR_CARRIER_DATA_TYPE;
        }
        else
        {
            p_ir_learn_para->ir_learn_buffer[i] = (p_ir_learn_para->ir_learn_buffer[i] & IR_DATA_MSK) *
                                                  p_ir_learn_para->carrier_freq / IR_LEARN_FREQ;
        }

        /*ir learn buffer data must minus 1 for writing to register*/
        if ((p_ir_learn_para->ir_learn_buffer[i] & IR_DATA_MSK) > 0)
        {
            p_ir_learn_para->ir_learn_buffer[i] -= 1;
        }
    }
}

/******************************************************************
 * @brief  learn a specific IR waveform freqency.
 * @param  p_ir_learn_para - point to IR packet struct.
 * @return the learning status.
 * @retval T_IR_LEARN_RESULT
 */
T_IR_LEARN_RESULT ir_learn_freq(T_IR_LEARN_PARA *p_ir_learn_para)
{
    uint16_t i = 0;
    uint16_t j = 0;
    float freq_sum = 0;
#if FILTER_IR_LEARN_FREQ
    float max_freq = 0;
    float min_freq = 2000000;
    float temp = 0;
#endif
    for (i = 0, j = 0; (i <= p_ir_learn_para->ir_learn_buf_index) &&
         (j <= p_ir_learn_para->carrier_info_idx); i++)
    {
        if (p_ir_learn_para->ir_learn_buffer[i] & IR_CARRIER_DATA_TYPE)
        {
            temp = (p_ir_learn_para->carrier_info_buf[j++] * (IR_LEARN_FREQ * 0.5)) / \
                   (p_ir_learn_para->ir_learn_buffer[i] & IR_DATA_MSK);

            freq_sum += temp;

#if FILTER_IR_LEARN_FREQ
            if (max_freq < temp)
            {
                max_freq = temp;
            }

            if (min_freq > temp)
            {
                min_freq = temp;
            }
#endif
        }
    }
#if FILTER_IR_LEARN_FREQ

    if (j > 2)
    {
        freq_sum -= (max_freq + min_freq);
        j -= 2;
    }
#endif
    if (j)
    {
        p_ir_learn_para->carrier_freq = freq_sum / j;
    }

    return IR_LEARN_OK;
}

/******************************************************************
 * @brief  learn a specific IR waveform.
 * @param  p_ir_learn_para - point to IR packet struct.
 * @return the decoded status.
 * @retval T_IR_LEARN_RESULT
 */
T_IR_LEARN_RESULT ir_learn_decode(T_IR_LEARN_PARA *p_ir_learn_para)
{
    uint32_t time_interval_buf[IR_RX_GDMA_FRAME_SIZE + IR_RX_FIFO_SIZE] = {0};
    uint32_t decode_data_len = 0;//*4byte

    if (ir_learn_struct.ir_learn_result != IR_LEARN_OK)
    {
        LOG_DBG("[IR] ir_learn_decode ir_learn_result = %d", ir_learn_struct.ir_learn_result);
        return ir_learn_struct.ir_learn_result;
    }

#if IR_LEARN_DUTY_CYCLE_SUPPORT
    if (p_ir_learn_para->duty_cycle <= 0)
    {
        ir_learn_struct.ir_learn_result = ir_learn_decode_duty_cycle(p_ir_learn_para);

        if (ir_learn_struct.ir_learn_result != IR_LEARN_OK)
        {
            return ir_learn_struct.ir_learn_result;
        }
    }
#endif

    if (ir_learn_struct.p_ir_learn_data->ir_learn_stop_flag)
    {
        decode_data_len = ir_learn_struct.p_ir_learn_data->remaining_gdma_data_len +
                          ir_learn_struct.p_ir_learn_data->remaining_fifo_data_len;
        memcpy(time_interval_buf,
               ir_learn_struct.p_ir_learn_data->gdma_buf_data[ir_learn_struct.p_ir_learn_data->out_buff_total_num %
                                                                                                                  IR_RX_GDMA_LINK_LIST_NUM].buf,
               ir_learn_struct.p_ir_learn_data->remaining_gdma_data_len * sizeof(uint32_t));
        memcpy(time_interval_buf + ir_learn_struct.p_ir_learn_data->remaining_gdma_data_len,
               ir_learn_struct.p_ir_learn_data->remaining_fifo_data,
               ir_learn_struct.p_ir_learn_data->remaining_fifo_data_len * sizeof(uint32_t));
    }
    else
    {
        decode_data_len = IR_RX_GDMA_FRAME_SIZE;
        memcpy(time_interval_buf,
               ir_learn_struct.p_ir_learn_data->gdma_buf_data[ir_learn_struct.p_ir_learn_data->out_buff_total_num %
                                                                                                                  IR_RX_GDMA_LINK_LIST_NUM].buf, decode_data_len * sizeof(uint32_t));
    }
    ir_learn_struct.p_ir_learn_data->gdma_buf_data[(ir_learn_struct.p_ir_learn_data->out_buff_total_num++)
                                                   % IR_RX_GDMA_LINK_LIST_NUM].buf_full_flag = false;

    LOG_DBG("[IR] out_buff_total_num = %d",
                    ir_learn_struct.p_ir_learn_data->out_buff_total_num);

    for (uint32_t i = 0; i < decode_data_len; i++)
    {
        /* Extract data */
        time_interval_buf[i] &= IR_DATA_MSK;

        /* Check the maximum number of learning data */
        if (p_ir_learn_para->ir_learn_buf_index >= IR_LEARN_WAVE_MAX_LEN)
        {
            ir_learn_struct.ir_learn_result = IR_LEARN_EXCEED_SIZE;
            break;
        }

        /* Record total time of carrier wave */
        if (time_interval_buf[i] < IR_LEARN_TIME_HIGHEST_VALUE)
        {
            /* Record carrier waveform time */
            p_ir_learn_para->ir_learn_buffer[p_ir_learn_para->ir_learn_buf_index] += time_interval_buf[i];

            /* Record carrier number */
            p_ir_learn_para->carrier_info_buf[p_ir_learn_para->carrier_info_idx]++;

            /* Record data type */
            p_ir_learn_para->is_carrier = true;
#if IR_LEARN_DUTY_CYCLE_SUPPORT
            p_ir_learn_para->last_handle_data = time_interval_buf[i];
#endif
        }
        else
        {
            if (p_ir_learn_para->is_carrier == true)
            {
                /* Record carrier number */
                p_ir_learn_para->carrier_info_buf[p_ir_learn_para->carrier_info_idx]++;

#if IR_LEARN_DUTY_CYCLE_SUPPORT
                if (p_ir_learn_para->duty_cycle)
                {
                    if (p_ir_learn_para->last_handle_data < IR_LEARN_TIME_HIGHEST_VALUE)
                    {
                        p_ir_learn_para->ir_learn_buffer[p_ir_learn_para->ir_learn_buf_index] +=
                            p_ir_learn_para->carrier_time -
                            p_ir_learn_para->last_handle_data;
                    }
                }
                else
                {
                    LOG_DBG("[IR] Warning: IR carrier data compensation handle error!");
                    ir_learn_struct.ir_learn_result =  IR_LEARN_CARRIRE_DATA_HANDLE_ERR;
                }
#endif
                LOG_DBG("[IR] high power time, index[%d]: %d", p_ir_learn_para->ir_learn_buf_index,
                                p_ir_learn_para->ir_learn_buffer[p_ir_learn_para->ir_learn_buf_index]);
                /* Store acitve carriar data, set IR_TX_DATA_TYPE*/
                p_ir_learn_para->ir_learn_buffer[p_ir_learn_para->ir_learn_buf_index++] |= IR_CARRIER_DATA_TYPE;

                /* Check the maximum number of learning data */
                if (p_ir_learn_para->ir_learn_buf_index >= IR_LEARN_WAVE_MAX_LEN)
                {
                    ir_learn_struct.ir_learn_result = IR_LEARN_EXCEED_SIZE;
                    break;
                }

#if IR_LEARN_DUTY_CYCLE_SUPPORT
                if (p_ir_learn_para->duty_cycle)
                {
                    /* Store value of low waveform */
                    p_ir_learn_para->ir_learn_buffer[p_ir_learn_para->ir_learn_buf_index++] = time_interval_buf[i] +
                                                                                              p_ir_learn_para->last_handle_data - \
                                                                                              p_ir_learn_para->carrier_time;
                }
                else
                {
                    /* Store value of low waveform */
                    p_ir_learn_para->ir_learn_buffer[p_ir_learn_para->ir_learn_buf_index++] = time_interval_buf[i];
                    LOG_DBG("[IR] Warning: IR no carrier data compensation handle error!");
                    ir_learn_struct.ir_learn_result = IR_LEARN_NO_CARRIRE_DATA_HANDLE_ERR;
                }
#else
                /* Store inactive carrier data*/
                p_ir_learn_para->ir_learn_buffer[p_ir_learn_para->ir_learn_buf_index++] = time_interval;
#endif
                LOG_DBG("[IR] low power time, index[%d]: %d", p_ir_learn_para->ir_learn_buf_index - 1,
                                p_ir_learn_para->ir_learn_buffer[p_ir_learn_para->ir_learn_buf_index - 1]);

                /* Record new carrier waveform information */
                p_ir_learn_para->carrier_info_idx++;
                /* Record data type */
                p_ir_learn_para->is_carrier = false;
            }
            else
            {
                LOG_DBG("[IR] Warning: exceed maximum stop signal value!");
                ir_learn_struct.ir_learn_result = IR_LEARN_WAVEFORM_ERR;
                break;
            }
        }

        /* Check the maximum number of learning data */
        if (p_ir_learn_para->ir_learn_buf_index >= IR_LEARN_WAVE_MAX_LEN)
        {
            ir_learn_struct.ir_learn_result = IR_LEARN_EXCEED_SIZE;
            break;
        }

        /* Check IR end signal */
        if (time_interval_buf[i] >= IR_LEARN_STOP_TIME)
        {
            ir_learn_struct.ir_learn_result = IR_LEARN_EXIT;
            break;
        }
    }

    return ir_learn_struct.ir_learn_result;
}

/******************************************************************
 * @brief   Check ir learn if working.
 * @param   none
 * @return  if ir is workging for learning
 * @retval  true or false
 */
bool ir_learn_is_working(void)
{
    if (ir_learn_struct.ir_learn_state == IR_LEARN_IDLE)
    {
        return false;
    }
    return true;
}

/******************************************************************
 * @brief   get ir learn current state
 * @param   none
 * @return  ir learn current state
 * @retval  T_IR_LEARN_RESULT
 */
T_IR_LEARN_STATE ir_learn_get_current_state(void)
{
    return ir_learn_struct.ir_learn_state;
}

/******************************************************************
 * @brief   Initializes IR learn data.
 * @param   p_ir_learn_para
 * @return  none
 * @retval  void
 */
void ir_learn_data_init(T_IR_LEARN_PARA *p_ir_learn_para)
{
    memset(p_ir_learn_para, 0, sizeof(T_IR_LEARN_PARA));

    // for (uint8_t i = 0; i < IR_RX_GDMA_LINK_LIST_NUM; i++)
    // {

    //     p_ir_learn_para->gdma_buf_data[i].buf = os_mem_alloc(RAM_TYPE_DATA_ON,
    //                                                          IR_RX_GDMA_FRAME_SIZE * sizeof(uint32_t));
    //     if (p_ir_learn_para->gdma_buf_data[i].buf != NULL)
    //     {
    //         memset((uint8_t *)p_ir_learn_para->gdma_buf_data[i].buf, 0,
    //                IR_RX_GDMA_FRAME_SIZE * sizeof(uint32_t));
    //     }
    //     else
    //     {
    //         os_mem_free(p_ir_learn_para->gdma_buf_data[i].buf);
    //         p_ir_learn_para->gdma_buf_data[i].buf = NULL;
    //     }
    // }
#if !IR_LEARN_DUTY_CYCLE_SUPPORT
    ir_learn_struct.p_ir_learn_data->duty_cycle = IR_DUTY_CYCLE;
#endif
}

/******************************************************************
 * @brief   IR learn module init.
 * @param   p_ir_learn_para
 * @return  none
 * @retval  void
 */
void ir_learn_module_init(T_IR_LEARN_PARA *p_ir_learn_para)
{
	LOG_DBG("[IR] ir learn init.");

    k_timer_start(&ir_learn_timer, K_MSEC(IR_LEARN_TIMEOUT), K_NO_WAIT);
    ir_learn_struct.p_ir_learn_data = p_ir_learn_para;
    ir_learn_struct.ir_learn_state = IR_LEARN_READY;
	k_work_init(&ir_learn_msg_processor.work, ir_learn_msg_proc);
}

/******************************************************************
 * @brief   start ir learning
 * @param   none
 * @return  none
 * @retval  void
 */
void ir_learn_start(void)
{
    if (ir_learn_struct.ir_learn_state != IR_LEARN_WORKING)
    {
        LOG_DBG("[IR] ir learn mode start.");
        ir_learn_data_init(ir_learn_struct.p_ir_learn_data);
        ir_learn_struct.ir_learn_state = IR_LEARN_WORKING;
        ir_learn_struct.ir_learn_result = IR_LEARN_OK;
    }
    else
    {
        LOG_DBG("[IR] ir learn mode already started.");
    }
}

/******************************************************************
 * @brief   stop ir learning
 * @param   none
 * @return  none
 * @retval  void
 */
void ir_learn_stop(void)
{
    if (ir_learn_struct.ir_learn_state == IR_LEARN_WORKING)
    {
        LOG_DBG("[IR] ir learn mode stop.");
        ir_learn_struct.ir_learn_state = IR_LEARN_READY;
        ir_rx_disable(ir_dev, NULL);
        // for (uint8_t i = 0; i < IR_RX_GDMA_LINK_LIST_NUM; i++)
        // {
        //     if (ir_learn_struct.p_ir_learn_data->gdma_buf_data[i].buf != NULL)
        //     {
        //         LOG_DBG("[IR] ir_learn_stop: mem_free_buf");
        //         os_mem_free(ir_learn_struct.p_ir_learn_data->gdma_buf_data[i].buf);
        //         ir_learn_struct.p_ir_learn_data->gdma_buf_data[i].buf = NULL;
        //     }
        // }
    }
    else
    {
        LOG_DBG("[IR] ir learn mode is not working, can not stop.");
    }
}

/******************************************************************
 * @brief   exit ir learning
 * @param   none
 * @return  none
 * @retval  void
 */
void ir_learn_exit(void)
{
    if (ir_learn_struct.ir_learn_state != IR_LEARN_IDLE)
    {
        LOG_DBG("[IR] ir learn mode exit.");
        ir_learn_struct.ir_learn_state = IR_LEARN_IDLE;
        ir_rx_disable(ir_dev, NULL);

        // for (uint8_t i = 0; i < IR_RX_GDMA_LINK_LIST_NUM; i++)
        // {
        //     if (ir_learn_struct.p_ir_learn_data->gdma_buf_data[i].buf != NULL)
        //     {
        //         LOG_DBG("[IR] ir_learn_exit: mem_free_buf");
        //         os_mem_free(ir_learn_struct.p_ir_learn_data->gdma_buf_data[i].buf);
        //         ir_learn_struct.p_ir_learn_data->gdma_buf_data[i].buf = NULL;
        //     }
        // }
    }
    else
    {
        LOG_DBG("[IR] ir learn mode already exited.");
    }
}

#endif