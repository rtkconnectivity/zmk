/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/**
 * The method by which data is sent.
 */
enum zmk_transport {
    ZMK_TRANSPORT_USB,
    ZMK_TRANSPORT_BLE,
    ZMK_TRANSPORT_PPT,
};

/**
 * Configuration to select an endpoint on ZMK_TRANSPORT_USB.
 */
struct zmk_transport_usb_data {};

/**
 * Configuration to select an endpoint on ZMK_TRANSPORT_BLE.
 */
struct zmk_transport_ble_data {
    int profile_index;
};

/**
 * Configuration to select an endpoint on ZMK_TRANSPORT_PPT.
 */
struct zmk_transport_ppt_data {};

/**
 * A specific endpoint to which data may be sent.
 */
struct zmk_endpoint_instance {
    enum zmk_transport transport;
    union {
        struct zmk_transport_usb_data usb; // ZMK_TRANSPORT_USB
        struct zmk_transport_ble_data ble; // ZMK_TRANSPORT_BLE
        struct zmk_transport_ppt_data ppt; // ZMK_TRANSPORT_PPT
    };
};
