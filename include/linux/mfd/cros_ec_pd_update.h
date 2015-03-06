/*
 * cros_ec_pd - Chrome OS EC Power Delivery Device Driver
 *
 * Copyright (C) 2014 Google, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_MFD_CROS_EC_PD_H
#define __LINUX_MFD_CROS_EC_PD_H

#include <linux/mfd/cros_ec_commands.h>

enum cros_ec_pd_device_type {
	PD_DEVICE_TYPE_NONE = 0,
	PD_DEVICE_TYPE_ZINGER = 1,
	PD_DEVICE_TYPE_COUNT,
};

/* PD image size is 16k. */
#define PD_RW_IMAGE_SIZE (16 * 1024)

#define USB_VID_GOOGLE 0x18d1

#define USB_PID_ZINGER 0x5012

struct cros_ec_pd_firmware_image {
	unsigned int id_major;
	unsigned int id_minor;
	uint16_t usb_vid;
	uint16_t usb_pid;
	char *filename;
	uint8_t hash[PD_RW_HASH_SIZE];
};

struct cros_ec_pd_update_data {
	struct device *dev;

	struct delayed_work work;
	struct workqueue_struct *workqueue;

	int num_ports;
};

#define PD_ID_MAJOR_SHIFT 0
#define PD_ID_MAJOR_MASK  0x03ff
#define PD_ID_MINOR_SHIFT 10
#define PD_ID_MINOR_MASK  0xfc00

#define MAJOR_MINOR_TO_DEV_ID(major, minor) \
	((((major) << PD_ID_MAJOR_SHIFT) & PD_ID_MAJOR_MASK) | \
	(((minor) << PD_ID_MINOR_SHIFT) & PD_ID_MINOR_MASK))

#define PD_NO_IMAGE -1

/* Send 96 bytes per write command when flashing PD device */
#define PD_FLASH_WRITE_STEP 96

/*
 * Wait 1s to start an update check after scheduling. This helps to remove
 * needless extra update checks (ex. if a PD device is reset several times
 * immediately after insertion) and fixes load issues on resume.
 */
#define PD_UPDATE_CHECK_DELAY msecs_to_jiffies(1000)

#endif /* __LINUX_MFD_CROS_EC_H */
