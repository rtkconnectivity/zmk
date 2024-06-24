/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <trace.h>
#include <zmk/ppt/ppt_sync_app.h>
#include <zephyr/logging/log.h>
#include <zmk/mode_monitor.h>
#include <rtl_enh_tim.h>
#include "power_manager_unit_platform.h"
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/*============================================================================*
 *                              Variables
 *============================================================================*/
static ppt_sync_app_t ppt_sync_app;

volatile ENHTIMStoreReg_Typedef ENHTIM_StoreReg;

/*============================================================================*
*                               Functions Declaration
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

/*============================================================================*
*                               Global Functions
*============================================================================*/
/**
 * @brief  Regist 2.4g message send callback
 * @param  cb - send callback, will be called after 2.4g data sending finish
 * @return None
 */
void sync_msg_reg_send_cb(sync_msg_send_cb_t cb)
{
    ppt_sync_app.msg_send_cb = cb;
}

#ifdef CONFIG_PM_DEVICE

extern void ENHTIM_DLPSEnter(void *PeriReg, void *StoreBuf);
extern void ENHTIM_DLPSExit(void *PeriReg, void *StoreBuf);

static int sync_entim_enter_dlps_cb()
{
    ENHTIM_DLPSEnter(ENH_TIM0, (void *)&ENHTIM_StoreReg);
    return 0;
}
static int sync_entim_exit_dlps_cb()
{
    ENHTIM_DLPSExit(ENH_TIM0, (void *)&ENHTIM_StoreReg);
    return 0;
}

static void sync_entim_dlps_init()
{
    /* 2.4g need ensure the entimer restore early than 2.4g restore, and 2.4g restore is in platform
    pend stage, so we register entimer restore to platform restore stage which has higher priority than
    pend stage */
    platform_pm_register_callback_func((void *)sync_entim_enter_dlps_cb, PLATFORM_PM_STORE);
    platform_pm_register_callback_func((void *)sync_entim_exit_dlps_cb, PLATFORM_PM_RESTORE);
}
#endif
/**
 * @brief  Initialize 2.4g proprietary
 * @param  role - 2.4g role, master or slave
 * @return None
 */
void ppt_sync_init(sync_role_t role)
{
    DBG_DIRECT("ppt_sync_init");
    memset(&ppt_sync_app, 0, sizeof(ppt_sync_app));
    sync_init(role);

#ifdef CONFIG_PM_DEVICE
    sync_dlps_init();
    sync_entim_dlps_init();
#endif
}

/**
 * @brief  Enable 2.4g proprietary after ppt_sync_init()
 * @param  None
 * @return None
 */
void ppt_sync_enable(void)
{
    DBG_DIRECT("ppt_sync_enable");
    sync_enable();
}

/**
 * @brief  Check if 2.4g is bonded
 * @param  None
 * @return Result - true: success, false: fail
 */
bool ppt_check_is_bonded(void)
{
    if (SYNC_ERR_CODE_SUCCESS == sync_nvm_get_bond_info(&ppt_sync_app.bond_info))
    {
        if (ppt_sync_app.bond_info.acc.addr != 0)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief  Start 2.4g pairing
 * @param  None
 * @return Result - true: success, false: fail
 */
bool ppt_pair(void)
{
    DBG_DIRECT("ppt_pair");
    sync_err_code_t ret = sync_pair();

    if (ret != SYNC_ERR_CODE_SUCCESS)
    {
        DBG_DIRECT("ppt_pair: fail, error code %d", ret);
        return false;
    }
    return true;
}

/**
 * @brief  Start 2.4g reconnecting
 * @param  None
 * @return Result - true: success, false: fail
 */
bool ppt_reconnect(void)
{
    DBG_DIRECT("ppt_reconnect");
    sync_err_code_t ret = sync_nvm_get_bond_info(&ppt_sync_app.bond_info);

    if (ret != SYNC_ERR_CODE_SUCCESS)
    {
        DBG_DIRECT("ppt_reconnect: get bond info fail, error code %d", ret);
        return false;
    }
    ret = sync_connect(&ppt_sync_app.bond_info);
    if (ret != SYNC_ERR_CODE_SUCCESS)
    {
        DBG_DIRECT("ppt_reconnect: fail, error code %d", ret);
        return false;
    }
    return true;
}

/**
 * @brief  2.4g stop sync stauts turn back to idle.
 * @param  None
 * @return None
 */
void ppt_stop_sync(void)
{
    DBG_DIRECT("ppt_stop_sync");
    sync_stop();
}

/**
 * @brief  Clear 2.4g bond information
 * @param  None
 * @return Result - true: success, false: fail
 */
bool ppt_clear_bond_info(void)
{
    sync_err_code_t ret = sync_nvm_clear_bond_info();
    if (ret == SYNC_ERR_CODE_SUCCESS)
    {
        memset(&ppt_sync_app.bond_info, 0, sizeof(ppt_sync_app.bond_info));
        DBG_DIRECT("ppt_clear_bond_info: success");
    }
    else
    {
        DBG_DIRECT("ppt_clear_bond_info: fail, error code %d", ret);
        return false;
    }
    return true;
}

/**
 * @brief  Send 2.4g data
 * @param  type - 2.4g data type for distinguishing between different types of data
 * @param  msg_retrans_count - retrans count for SYNC_MSG_TYPE_FINITE_RETRANS type of data
 * @param  data - pointer to 2.4g data
 * @param  len - data length
 * @return Send result - 0: success, others: fail reason
 */
sync_err_code_t ppt_app_send_data(sync_msg_type_t type, uint8_t msg_retrans_count,
                                  uint8_t *data, uint16_t len)
{
    DBG_DIRECT("ppt app send data");
    if (type == SYNC_MSG_TYPE_DYNAMIC_RETRANS)
    {
        return sync_msg_send(SYNC_MSG_TYPE_DYNAMIC_RETRANS, data, len, ppt_sync_app.msg_send_cb);
    }
    else if (type == SYNC_MSG_TYPE_INFINITE_RETRANS)
    {
        return sync_msg_send(SYNC_MSG_TYPE_INFINITE_RETRANS, data, len, ppt_sync_app.msg_send_cb);
    }
    else if (type == SYNC_MSG_TYPE_FINITE_RETRANS)
    {
        if (ppt_sync_app.msg_retrans_count != msg_retrans_count)
        {
            sync_msg_set_finite_retrans(msg_retrans_count);
            ppt_sync_app.msg_retrans_count = msg_retrans_count;
        }
        return sync_msg_send(SYNC_MSG_TYPE_FINITE_RETRANS, data, len, ppt_sync_app.msg_send_cb);
    }
    return SYNC_ERR_CODE_UNKNOWN;
}

