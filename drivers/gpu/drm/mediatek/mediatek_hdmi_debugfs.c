/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Jie Qiu <jie.qiu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#if defined(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#include <drm/drm_edid.h>
#include "mediatek_hdmi_ctrl.h"

#define  HELP_INFO \
	"\n" \
	"USAGE\n" \
	"        echo [ACTION]... > mtk_hdmi\n" \
	"\n" \
	"ACTION\n" \
	"\n" \
	"        res=fmt:\n" \
	"             set hdmi output video format\n" \
	"             for example, 'echo res=15 > mtk_hdmi' command will let HDMI output 1080P60HZ format\n" \
	"        getedid:\n" \
	"             'echo getedid > mtk_hdmi' command get edid of sink\n"


static struct drm_display_mode display_mode[] = {
	/* 1 - 640x480@60Hz */
	{DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 25175, 640, 656,
		  752, 800, 0, 480, 490, 492, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 2 - 720x480@60Hz */
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736,
		  798, 858, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 3 - 720x480@60Hz */
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736,
		  798, 858, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 4 - 1280x720@60Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1390,
		  1430, 1650, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 5 - 1920x1080i@60Hz */
	{DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2008,
		  2052, 2200, 0, 1080, 1084, 1094, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 6 - 720(1440)x480i@60Hz */
	{DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 13500, 720, 739,
		  801, 858, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 7 - 720(1440)x480i@60Hz */
	{DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 13500, 720, 739,
		  801, 858, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 8 - 720(1440)x240@60Hz */
	{DRM_MODE("720x240", DRM_MODE_TYPE_DRIVER, 13500, 720, 739,
		  801, 858, 0, 240, 244, 247, 262, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 9 - 720(1440)x240@60Hz */
	{DRM_MODE("720x240", DRM_MODE_TYPE_DRIVER, 13500, 720, 739,
		  801, 858, 0, 240, 244, 247, 262, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 10 - 2880x480i@60Hz */
	{DRM_MODE("2880x480i", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2956,
		  3204, 3432, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 11 - 2880x480i@60Hz */
	{DRM_MODE("2880x480i", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2956,
		  3204, 3432, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 12 - 2880x240@60Hz */
	{DRM_MODE("2880x240", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2956,
		  3204, 3432, 0, 240, 244, 247, 262, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 13 - 2880x240@60Hz */
	{DRM_MODE("2880x240", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2956,
		  3204, 3432, 0, 240, 244, 247, 262, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 14 - 1440x480@60Hz */
	{DRM_MODE("1440x480", DRM_MODE_TYPE_DRIVER, 54000, 1440, 1472,
		  1596, 1716, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 15 - 1440x480@60Hz */
	{DRM_MODE("1440x480", DRM_MODE_TYPE_DRIVER, 54000, 1440, 1472,
		  1596, 1716, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 16 - 1920x1080@60Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
		  2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 17 - 720x576@50Hz */
	{DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 27000, 720, 732,
		  796, 864, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 18 - 720x576@50Hz */
	{DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 27000, 720, 732,
		  796, 864, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 19 - 1280x720@50Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1720,
		  1760, 1980, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 20 - 1920x1080i@50Hz */
	{DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2448,
		  2492, 2640, 0, 1080, 1084, 1094, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 21 - 720(1440)x576i@50Hz */
	{DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 13500, 720, 732,
		  795, 864, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 22 - 720(1440)x576i@50Hz */
	{DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 13500, 720, 732,
		  795, 864, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 23 - 720(1440)x288@50Hz */
	{DRM_MODE("720x288", DRM_MODE_TYPE_DRIVER, 13500, 720, 732,
		  795, 864, 0, 288, 290, 293, 312, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 24 - 720(1440)x288@50Hz */
	{DRM_MODE("720x288", DRM_MODE_TYPE_DRIVER, 13500, 720, 732,
		  795, 864, 0, 288, 290, 293, 312, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 25 - 2880x576i@50Hz */
	{DRM_MODE("2880x576i", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2928,
		  3180, 3456, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 26 - 2880x576i@50Hz */
	{DRM_MODE("2880x576i", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2928,
		  3180, 3456, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 27 - 2880x288@50Hz */
	{DRM_MODE("2880x288", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2928,
		  3180, 3456, 0, 288, 290, 293, 312, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 28 - 2880x288@50Hz */
	{DRM_MODE("2880x288", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2928,
		  3180, 3456, 0, 288, 290, 293, 312, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 29 - 1440x576@50Hz */
	{DRM_MODE("1440x576", DRM_MODE_TYPE_DRIVER, 54000, 1440, 1464,
		  1592, 1728, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 30 - 1440x576@50Hz */
	{DRM_MODE("1440x576", DRM_MODE_TYPE_DRIVER, 54000, 1440, 1464,
		  1592, 1728, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 31 - 1920x1080@50Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2448,
		  2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 32 - 1920x1080@24Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2558,
		  2602, 2750, 0, 1080, 1084, 1089, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 24, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 33 - 1920x1080@25Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2448,
		  2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 25, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 34 - 1920x1080@30Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2008,
		  2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 30, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 35 - 2880x480@60Hz */
	{DRM_MODE("2880x480", DRM_MODE_TYPE_DRIVER, 108000, 2880, 2944,
		  3192, 3432, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 36 - 2880x480@60Hz */
	{DRM_MODE("2880x480", DRM_MODE_TYPE_DRIVER, 108000, 2880, 2944,
		  3192, 3432, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 60, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 37 - 2880x576@50Hz */
	{DRM_MODE("2880x576", DRM_MODE_TYPE_DRIVER, 108000, 2880, 2928,
		  3184, 3456, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 38 - 2880x576@50Hz */
	{DRM_MODE("2880x576", DRM_MODE_TYPE_DRIVER, 108000, 2880, 2928,
		  3184, 3456, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 39 - 1920x1080i@50Hz */
	{DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 72000, 1920, 1952,
		  2120, 2304, 0, 1080, 1126, 1136, 1250, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 50, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 40 - 1920x1080i@100Hz */
	{DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2448,
		  2492, 2640, 0, 1080, 1084, 1094, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 41 - 1280x720@100Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 148500, 1280, 1720,
		  1760, 1980, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 42 - 720x576@100Hz */
	{DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 54000, 720, 732,
		  796, 864, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 43 - 720x576@100Hz */
	{DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 54000, 720, 732,
		  796, 864, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 44 - 720(1440)x576i@100Hz */
	{DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 27000, 720, 732,
		  795, 864, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 45 - 720(1440)x576i@100Hz */
	{DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 27000, 720, 732,
		  795, 864, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 46 - 1920x1080i@120Hz */
	{DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
		  2052, 2200, 0, 1080, 1084, 1094, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
		  DRM_MODE_FLAG_INTERLACE),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 47 - 1280x720@120Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 148500, 1280, 1390,
		  1430, 1650, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 48 - 720x480@120Hz */
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 54000, 720, 736,
		  798, 858, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 49 - 720x480@120Hz */
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 54000, 720, 736,
		  798, 858, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 50 - 720(1440)x480i@120Hz */
	{DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 27000, 720, 739,
		  801, 858, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 51 - 720(1440)x480i@120Hz */
	{DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 27000, 720, 739,
		  801, 858, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 52 - 720x576@200Hz */
	{DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 108000, 720, 732,
		  796, 864, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 200, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 53 - 720x576@200Hz */
	{DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 108000, 720, 732,
		  796, 864, 0, 576, 581, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 200, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 54 - 720(1440)x576i@200Hz */
	{DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 54000, 720, 732,
		  795, 864, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 200, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 55 - 720(1440)x576i@200Hz */
	{DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 54000, 720, 732,
		  795, 864, 0, 576, 580, 586, 625, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 200, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 56 - 720x480@240Hz */
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 108000, 720, 736,
		  798, 858, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 240, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 57 - 720x480@240Hz */
	{DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 108000, 720, 736,
		  798, 858, 0, 480, 489, 495, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	 .vrefresh = 240, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 58 - 720(1440)x480i@240 */
	{DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 54000, 720, 739,
		  801, 858, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 240, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,},
	/* 59 - 720(1440)x480i@240 */
	{DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 54000, 720, 739,
		  801, 858, 0, 480, 488, 494, 525, 0,
		  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		  DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	 .vrefresh = 240, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 60 - 1280x720@24Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 59400, 1280, 3040,
		  3080, 3300, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 24, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 61 - 1280x720@25Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 3700,
		  3740, 3960, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 25, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 62 - 1280x720@30Hz */
	{DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 3040,
		  3080, 3300, 0, 720, 725, 730, 750, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 30, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 63 - 1920x1080@120Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 297000, 1920, 2008,
		  2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 120, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
	/* 64 - 1920x1080@100Hz */
	{DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 297000, 1920, 2448,
		  2492, 2640, 0, 1080, 1084, 1094, 1125, 0,
		  DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	 .vrefresh = 100, .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,},
};

static int mtk_hdmi_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mtk_hdmi_debug_read(struct file *file, char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	return simple_read_from_buffer(ubuf, count, ppos, HELP_INFO,
				       strlen(HELP_INFO));
}

static int mtk_hdmi_process_cmd(char *cmd, struct mediatek_hdmi_context *hctx)
{
	char *np;
	unsigned long res;

	mtk_hdmi_info("dbg cmd: %s\n", cmd);

	if (0 == strncmp(cmd, "getedid", 7)) {
		struct edid *edid_info = NULL;

		edid_info = drm_get_edid(&hctx->conn, hctx->ddc_adpt);
		if (!edid_info) {
			mtk_hdmi_err("get edid faied!\n");
			goto errcode;
		} else {
			int i = 0;
			u8 *edid_raw = (u8 *)edid_info;
			int size = EDID_LENGTH * (1 + edid_info->extensions);

			mtk_hdmi_output("get edid success! edid raw data:\n");
			for (i = 0; i < size; i++) {
				if (i % 8 == 0)
					mtk_hdmi_output("\n%02xH", i);
				mtk_hdmi_output(" %02x", edid_raw[i]);
			}
			mtk_hdmi_output("\n");
		}
	} else {
		np = strsep(&cmd, "=");
		if (0 != strncmp(np, "res", 3))
			goto errcode;

		np = strsep(&cmd, "=");
		if (kstrtoul(np, 10, &res))
			goto errcode;

		if (res >= ARRAY_SIZE(display_mode)) {
			mtk_hdmi_err("doesn't support this format, res = %ld\n", res);
			goto errcode;
		}

		mtk_hdmi_info("set format %ld\n", res);
		mtk_hdmi_display_set_vid_format(hctx->display_node, &display_mode[res]);
	}

	return 0;

errcode:
	mtk_hdmi_err("invalid dbg command\n");
	return -1;
}

static ssize_t mtk_hdmi_debug_write(struct file *file, const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	char str_buf[64];
	size_t ret;
	struct mediatek_hdmi_context *hctx;

	hctx = file->private_data;
	ret = count;
	memset(str_buf, 0, sizeof(str_buf));

	if (count > sizeof(str_buf))
		count = sizeof(str_buf);

	if (copy_from_user(str_buf, ubuf, count))
		return -EFAULT;

	str_buf[count] = 0;

	mtk_hdmi_process_cmd(str_buf, hctx);

	return ret;
}

static const struct file_operations mtk_hdmi_debug_fops = {
	.read = mtk_hdmi_debug_read,
	.write = mtk_hdmi_debug_write,
	.open = mtk_hdmi_debug_open,
};

int mtk_drm_hdmi_debugfs_init(struct mediatek_hdmi_context *hdmi_context)
{
	hdmi_context->debugfs =
	    debugfs_create_file("mtk_hdmi", S_IFREG | S_IRUGO |
				S_IWUSR | S_IWGRP, NULL, (void *)hdmi_context,
				&mtk_hdmi_debug_fops);

	if (IS_ERR(hdmi_context->debugfs))
		return PTR_ERR(hdmi_context->debugfs);

	return 0;
}

void mtk_drm_hdmi_debugfs_exit(struct mediatek_hdmi_context *hdmi_context)
{
	debugfs_remove(hdmi_context->debugfs);
}

#endif
