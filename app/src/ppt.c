/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/init.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>

#include <zmk/usb.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/ppt.h>
#include <zmk/event_manager.h>
#include <keyboard_ppt_app.h>
#include "trace.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static uint8_t *get_keyboard_report(size_t *len) {

    struct zmk_hid_keyboard_report *report = zmk_hid_get_keyboard_report();
    *len = sizeof(*report);
    return (uint8_t *)report;
}

static int zmk_ppt_send_report(uint8_t *report, size_t len) {
    for(uint8_t i=0; i<len; i++)
    {
        DBG_DIRECT("ppt send data:0x%x", report[i]);
    }
    ppt_app_send_data(SYNC_MSG_TYPE_DYNAMIC_RETRANS,0,report,len);
}

int zmk_ppt_send_keyboard_report(void) {
    size_t len;
    uint8_t *report = get_keyboard_report(&len);
    DBG_DIRECT("zmk ppt send keyboard data, len is %d",len);
    
    uint8_t opcode = SYNC_OPCODE_KEYBOARD;
    uint8_t ppt_report[len+1];
    ppt_report[0] = opcode;
    memcpy(&ppt_report[1], report, len);
    return zmk_ppt_send_report(ppt_report, (len+1));
}

int zmk_ppt_send_consumer_report(void) {
    DBG_DIRECT("zmk ppt send consumer data");
    struct zmk_hid_consumer_report *report = zmk_hid_get_consumer_report();
    uint16_t len = sizeof(*report);

    uint8_t opcode = SYNC_OPCODE_CONSUMER;
    uint8_t ppt_report[len+1];
    ppt_report[0] = opcode;
    memcpy(&ppt_report[1], report, len);
    return zmk_ppt_send_report(ppt_report, (len+1));
}

//SYS_INIT(zmk_ppt_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
