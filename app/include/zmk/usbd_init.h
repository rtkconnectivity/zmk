/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <zephyr/usb/usbd.h>

struct usbd_context *zmk_usbd_init_device(usbd_msg_cb_t msg_cb);
void zmk_usbd_wakeup_request(void);
