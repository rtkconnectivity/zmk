/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>

#include <zmk/event_manager.h>
#include <zmk/ppt.h>

struct zmk_ppt_conn_state_changed {
    enum zmk_ppt_conn_state conn_state;
};

ZMK_EVENT_DECLARE(zmk_ppt_conn_state_changed);