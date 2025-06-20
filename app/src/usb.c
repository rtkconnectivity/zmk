/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include "trace.h"
#include <zmk/events/usb_conn_state_changed.h>
#include <zephyr/drivers/gpio.h>
#include <zmk/usb_hid.h>
#include <zmk/mode_monitor.h>
#include <zmk/usbd_init.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct gpio_dt_spec detect_usb =
    GPIO_DT_SPEC_GET(DT_NODELABEL(mode_monitor), detect_usb_gpios);

static enum usb_dc_status_code usb_status = USB_DC_UNKNOWN;

static void raise_usb_status_changed_event(struct k_work *_work) {
    raise_zmk_usb_conn_state_changed(
        (struct zmk_usb_conn_state_changed){.conn_state = zmk_usb_get_conn_state()});
}
K_WORK_DEFINE(usb_status_notifier_work, raise_usb_status_changed_event);

enum usb_dc_status_code zmk_usb_get_status(void) { return usb_status; }

enum zmk_usb_conn_state zmk_usb_get_conn_state(void) {
    LOG_DBG("state: %d", usb_status);
    switch (usb_status) {
    case USB_DC_SUSPEND:
    case USB_DC_CONFIGURED:
    case USB_DC_RESUME:
    case USB_DC_CLEAR_HALT:
    case USB_DC_SOF:
        return ZMK_USB_CONN_HID;

    case USB_DC_DISCONNECTED:
    case USB_DC_UNKNOWN:
        return ZMK_USB_CONN_NONE;

    default:
        return ZMK_USB_CONN_POWERED;
    }
}

void usb_status_cb(enum usb_dc_status_code status, const uint8_t *params) {
    // Start-of-frame events are too frequent and noisy to notify, and they're
    // not used within ZMK
    LOG_DBG("usb status cb: usb status is %d", status);
    DBG_DIRECT("usb status cb: usb status is %d", status);
    if (status == USB_DC_SOF) {
        return;
    }

#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
    if (status == USB_DC_RESET) {
        zmk_usb_hid_set_protocol(HID_PROTOCOL_REPORT);
    }
#endif
    usb_status = status;
    if (status == USB_DC_CONFIGURED) {
        app_global_data.is_usb_enumeration_success = true;
    }
    k_work_submit(&usb_status_notifier_work);
};

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
static struct usbd_context *zmk_usbd;
/* doc device msg-cb start */
static void usb_msg_cb(struct usbd_context *const usbd_ctx,
		   const struct usbd_msg *const msg)
{
	LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

    switch(msg->type) {
        case USBD_MSG_SUSPEND:
        case USBD_MSG_CDC_ACM_LINE_CODING:
            usb_status = USB_DC_SUSPEND;
            break;
        case USBD_MSG_RESET: /* msg will come when usb enum done */
            usb_status = USB_DC_CONFIGURED;
            break;
        case USBD_MSG_RESUME:
            usb_status = USB_DC_RESUME;
            break;
        case USBD_MSG_VBUS_REMOVED:
        case USBD_MSG_UDC_ERROR:
            usb_status = USB_DC_DISCONNECTED;
            break;
        default:
            usb_status = USB_DC_UNKNOWN;
            break;
    }
    if (msg->type == USBD_MSG_RESET) {
        app_global_data.is_usb_enumeration_success = true;
    }
    k_work_submit(&usb_status_notifier_work);
	// if (usbd_can_detect_vbus(usbd_ctx)) {
	// 	if (msg->type == USBD_MSG_VBUS_READY) {
	// 		if (usbd_enable(usbd_ctx)) {
	// 			LOG_ERR("Failed to enable device support");
	// 		}
	// 	}

	// 	if (msg->type == USBD_MSG_VBUS_REMOVED) {
	// 		if (usbd_disable(usbd_ctx)) {
	// 			LOG_ERR("Failed to disable device support");
	// 		}
	// 	}
	// }
}
/* doc device msg-cb end */
static int enable_usb_device_next(void)
{
	int err;
	zmk_usbd = zmk_usbd_init_device(usb_msg_cb);
	if (zmk_usbd == NULL) {
		LOG_ERR("Failed to initialize USB device");
		return -ENODEV;
	}

	err = usbd_enable(zmk_usbd);
	if (err) {
		LOG_ERR("Failed to enable device support");
		return err;
	}

	LOG_DBG("USB device support enabled");

	return 0;
}
#endif

int zmk_usb_init(void) {
    int usb_enable_ret;
    int usb_disable_ret;
#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
	usb_enable_ret = enable_usb_device_next();
#else
	usb_enable_ret = usb_enable(usb_status_cb);
#endif
//     if (!gpio_pin_get_raw(detect_usb.port, detect_usb.pin)) {
//         LOG_DBG("usb is not insert");
// #if defined(CONFIG_USB_DEVICE_STACK_NEXT)
//         usb_disable_ret = usbd_disable(zmk_usbd);
// #else
//         usb_disable_ret = usb_disable();
// #endif
//         if (usb_disable_ret != 0) {
//             LOG_ERR("Unable to disable usb ,err = %d", usb_disable_ret);
//             return -EINVAL;
//         }
//     }

    if (usb_enable_ret != 0) {
        LOG_ERR("Unable to enable USB ,err = %d", usb_enable_ret);
        app_mode.is_in_usb_mode = false;
        return -EINVAL;
    }

    return 0;
}
int zmk_usb_deinit(void) {
    int usb_disable_ret;
#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
	usb_disable_ret = usbd_disable(zmk_usbd);
#else
	usb_disable_ret = usb_disable();
#endif

    if (usb_disable_ret != 0) {
        LOG_ERR("Unable to disable USB");
        return -EINVAL;
    }

    return 0;
}

SYS_INIT(zmk_usb_init, APPLICATION, CONFIG_ZMK_USB_INIT_PRIORITY);
