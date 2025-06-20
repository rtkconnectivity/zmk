/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* doc device instantiation start */
/*
 * Instantiate a context named zmk_usbd using the default USB device
 * controller.
 */
USBD_DEVICE_DEFINE(zmk_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_USBD_VID, CONFIG_USBD_PID);
/* doc device instantiation end */

/* doc string instantiation start */
USBD_DESC_LANG_DEFINE(zmk_usbd_LANG);
USBD_DESC_MANUFACTURER_DEFINE(zmk_usbd_mfr, CONFIG_USBD_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(zmk_usbd_product, CONFIG_USBD_PRODUCT);
USBD_DESC_SERIAL_NUMBER_DEFINE(zmk_usbd_sn);
/* doc string instantiation end */

/* doc configuration instantiation start */
static const uint8_t attributes = (IS_ENABLED(CONFIG_USBD_SELF_POWERED) ?
				   USB_SCD_SELF_POWERED : 0) |
				  (IS_ENABLED(CONFIG_USBD_REMOTE_WAKEUP) ?
				   USB_SCD_REMOTE_WAKEUP : 0);

/* Full speed configuration */
USBD_CONFIGURATION_DEFINE(zmk_usbd_fs_config,
			  attributes,
			  CONFIG_USBD_MAX_POWER);

/* High speed configuration */
USBD_CONFIGURATION_DEFINE(zmk_usbd_hs_config,
			  attributes,
			  CONFIG_USBD_MAX_POWER);
/* doc configuration instantiation end */

/*
 * This does not yet provide valuable information, but rather serves as an
 * example, and will be improved in the future.
 */
static const struct usb_bos_capability_lpm bos_cap_lpm = {
	.bLength = sizeof(struct usb_bos_capability_lpm),
	.bDescriptorType = USB_DESC_DEVICE_CAPABILITY,
	.bDevCapabilityType = USB_BOS_CAPABILITY_EXTENSION,
	.bmAttributes = 0UL,
};

USBD_DESC_BOS_DEFINE(zmk_usbext, sizeof(bos_cap_lpm), &bos_cap_lpm);

static void zmk_fix_code_triple(struct usbd_context *uds_ctx,
				   const enum usbd_speed speed)
{
	/* Always use class code information from Interface Descriptors */
	if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_AUDIO2_CLASS)) {
		/*
		 * Class with multiple interfaces have an Interface
		 * Association Descriptor available, use an appropriate triple
		 * to indicate it.
		 */
		usbd_device_set_code_triple(uds_ctx, speed,
					    USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	} else {
		usbd_device_set_code_triple(uds_ctx, speed, 0, 0, 0);
	}
}

struct usbd_context *zmk_usbd_init_device(usbd_msg_cb_t msg_cb)
{
	int err;

	/* doc add string descriptor start */
	err = usbd_add_descriptor(&zmk_usbd, &zmk_usbd_LANG);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&zmk_usbd, &zmk_usbd_mfr);
	if (err) {
		LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&zmk_usbd, &zmk_usbd_product);
	if (err) {
		LOG_ERR("Failed to initialize product descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&zmk_usbd, &zmk_usbd_sn);
	if (err) {
		LOG_ERR("Failed to initialize SN descriptor (%d)", err);
		return NULL;
	}
	/* doc add string descriptor end */

	if (usbd_caps_speed(&zmk_usbd) == USBD_SPEED_HS) {
		err = usbd_add_configuration(&zmk_usbd, USBD_SPEED_HS,
					     &zmk_usbd_hs_config);
		if (err) {
			LOG_ERR("Failed to add High-Speed configuration");
			return NULL;
		}

		err = usbd_register_all_classes(&zmk_usbd, USBD_SPEED_HS, 1);
		if (err) {
			LOG_ERR("Failed to add register classes");
			return NULL;
		}

		zmk_fix_code_triple(&zmk_usbd, USBD_SPEED_HS);
	}

	/* doc configuration register start */
	err = usbd_add_configuration(&zmk_usbd, USBD_SPEED_FS,
				     &zmk_usbd_fs_config);
	if (err) {
		LOG_ERR("Failed to add Full-Speed configuration");
		return NULL;
	}
	/* doc configuration register end */

	/* doc functions register start */
	err = usbd_register_all_classes(&zmk_usbd, USBD_SPEED_FS, 1);
	if (err) {
		LOG_ERR("Failed to add register classes");
		return NULL;
	}
	/* doc functions register end */

	zmk_fix_code_triple(&zmk_usbd, USBD_SPEED_FS);

	if (msg_cb != NULL) {
		/* doc device init-and-msg start */
		err = usbd_msg_register_cb(&zmk_usbd, msg_cb);
		if (err) {
			LOG_ERR("Failed to register message callback");
			return NULL;
		}
		/* doc device init-and-msg end */
	}

	if (IS_ENABLED(CONFIG_USBD_20_EXTENSION_DESC)) {
		(void)usbd_device_set_bcd(&zmk_usbd, USBD_SPEED_FS, 0x0201);
		(void)usbd_device_set_bcd(&zmk_usbd, USBD_SPEED_HS, 0x0201);

		err = usbd_add_descriptor(&zmk_usbd, &zmk_usbext);
		if (err) {
			LOG_ERR("Failed to add USB 2.0 Extension Descriptor");
			return NULL;
		}
	}

	/* doc device init start */
	err = usbd_init(&zmk_usbd);
	if (err) {
		LOG_ERR("Failed to initialize device support");
		return NULL;
	}
	/* doc device init end */

	return &zmk_usbd;
}

void zmk_usbd_wakeup_request(void)
{
	usbd_wakeup_request(&zmk_usbd);
}
