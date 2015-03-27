/*
 * Copyright (c) 2014 MediaTek Inc.
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

#include <linux/string.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/wait.h>

#include <drm/drmP.h>
#include "mediatek_drm_crtc.h"

/* ------------------------------------------------------------------------- */
/* External variable declarations */
/* ------------------------------------------------------------------------- */
static char debug_buffer[2048];
void __iomem *gdrm_disp_base[9];
void __iomem *gdrm_hdmi_base[5];
int gdrm_disp_table[9] = {
	0x14000000,
	0x1400c000,
	0x1400e000,
	0x14013000,
	0x14015000,
	0x1401a000,
	0x1401b000,
	0x14020000,
	0x14023000
};

int gdrm_hdmi_table[5] = {
	0x14000000,	/* CONFIG */
	0x1400d000,	/* OVL1 */
	0x1400f000,	/* RDMA1 */
	0x14014000,	/* COLOR1 */
	0x14016000	/* GAMMA */
};

/* ------------------------------------------------------------------------- */
/* Debug Options */
/* ------------------------------------------------------------------------- */
static char STR_HELP[] =
	"\n"
	"USAGE\n"
	"        echo [ACTION]... > mtkdrm\n"
	"\n"
	"ACTION\n"
	"\n"
	"        dump:\n"
	"             dump all hw registers\n"
	"\n"
	"        regw:addr=val\n"
	"             write hw register\n"
	"\n"
	"        regr:addr\n"
	"             read hw register\n";

/* ------------------------------------------------------------------------- */
/* Command Processor */
/* ------------------------------------------------------------------------- */
static void process_dbg_opt(const char *opt)
{
	if (0 == strncmp(opt, "regw:", 5)) {
		char *p = (char *)opt + 5;
		char *np;
		unsigned long addr, val;
		int i;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &addr))
			goto Error;

		if (p == NULL)
			goto Error;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &val))
			goto Error;

		for (i = 0; i < sizeof(gdrm_disp_table)/sizeof(int); i++) {
			if (addr > gdrm_disp_table[i] &&
			addr < gdrm_disp_table[i] + 0x1000) {
				addr -= gdrm_disp_table[i];
				writel(val, gdrm_disp_base[i] + addr);
				break;
			}
		}

		for (i = 0; i < sizeof(gdrm_hdmi_table)/sizeof(int); i++) {
			if (addr > gdrm_hdmi_table[i] &&
			addr < gdrm_hdmi_table[i] + 0x1000) {
				addr -= gdrm_hdmi_table[i];
				writel(val, gdrm_hdmi_base[i] + addr);
				break;
			}
		}
	} else if (0 == strncmp(opt, "regr:", 5)) {
		char *p = (char *)opt + 5;
		unsigned long addr;
		int i;

		if (kstrtoul(p, 16, &addr))
			goto Error;

		for (i = 0; i < sizeof(gdrm_disp_table)/sizeof(int); i++) {
			if (addr >= gdrm_disp_table[i] &&
			addr < gdrm_disp_table[i] + 0x1000) {
				addr -= gdrm_disp_table[i];
				DRM_INFO("Read register 0x%08X: 0x%08X\n",
					(unsigned int)addr + gdrm_disp_table[i],
					readl(gdrm_disp_base[i] + addr));
				break;
			}
		}

		for (i = 0; i < sizeof(gdrm_hdmi_table)/sizeof(int); i++) {
			if (addr >= gdrm_hdmi_table[i] &&
			addr < gdrm_hdmi_table[i] + 0x1000) {
				addr -= gdrm_hdmi_table[i];
				DRM_INFO("Read register 0x%08X: 0x%08X\n",
					(unsigned int)addr + gdrm_hdmi_table[i],
					readl(gdrm_hdmi_base[i] + addr));
				break;
			}
		}
	} else if (0 == strncmp(opt, "ovl0:", 5)) {
		int i;

		/* OVL */
		for (i = 0; i < 0x260; i += 16)
			DRM_INFO("OVL   0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[1] + i,
			readl(gdrm_disp_base[1] + i),
			readl(gdrm_disp_base[1] + i + 4),
			readl(gdrm_disp_base[1] + i + 8),
			readl(gdrm_disp_base[1] + i + 12));
		for (i = 0xf40; i < 0xfc0; i += 16)
			DRM_INFO("OVL   0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[1] + i,
			readl(gdrm_disp_base[1] + i),
			readl(gdrm_disp_base[1] + i + 4),
			readl(gdrm_disp_base[1] + i + 8),
			readl(gdrm_disp_base[1] + i + 12));
	} else if (0 == strncmp(opt, "dump:", 5)) {
		int i;

		/* CONFIG */
		for (i = 0; i < 0x120; i += 16)
			DRM_INFO("CFG   0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[0] + i,
			readl(gdrm_disp_base[0] + i),
			readl(gdrm_disp_base[0] + i + 4),
			readl(gdrm_disp_base[0] + i + 8),
			readl(gdrm_disp_base[0] + i + 12));

		/* OVL */
		for (i = 0; i < 0x260; i += 16)
			DRM_INFO("OVL   0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[1] + i,
			readl(gdrm_disp_base[1] + i),
			readl(gdrm_disp_base[1] + i + 4),
			readl(gdrm_disp_base[1] + i + 8),
			readl(gdrm_disp_base[1] + i + 12));
		for (i = 0xf40; i < 0xfc0; i += 16)
			DRM_INFO("OVL   0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[1] + i,
			readl(gdrm_disp_base[1] + i),
			readl(gdrm_disp_base[1] + i + 4),
			readl(gdrm_disp_base[1] + i + 8),
			readl(gdrm_disp_base[1] + i + 12));

		/* RDMA */
		for (i = 0; i < 0x100; i += 16)
			DRM_INFO("RDMA  0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[2] + i,
			readl(gdrm_disp_base[2] + i),
			readl(gdrm_disp_base[2] + i + 4),
			readl(gdrm_disp_base[2] + i + 8),
			readl(gdrm_disp_base[2] + i + 12));

		/* COLOR0 */
		for (i = 0x400; i < 0x500; i += 16)
			DRM_INFO("COLOR 0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[3] + i,
			readl(gdrm_disp_base[3] + i),
			readl(gdrm_disp_base[3] + i + 4),
			readl(gdrm_disp_base[3] + i + 8),
			readl(gdrm_disp_base[3] + i + 12));
		for (i = 0xC00; i < 0xD00; i += 16)
			DRM_INFO("COLOR 0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[3] + i,
			readl(gdrm_disp_base[3] + i),
			readl(gdrm_disp_base[3] + i + 4),
			readl(gdrm_disp_base[3] + i + 8),
			readl(gdrm_disp_base[3] + i + 12));

		/* AAL */
		for (i = 0; i < 0x100; i += 16)
			DRM_INFO("AAL   0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[4] + i,
			readl(gdrm_disp_base[4] + i),
			readl(gdrm_disp_base[4] + i + 4),
			readl(gdrm_disp_base[4] + i + 8),
			readl(gdrm_disp_base[4] + i + 12));

		/* UFOE */
		for (i = 0; i < 0x100; i += 16)
			DRM_INFO("UFOE  0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[5] + i,
			readl(gdrm_disp_base[5] + i),
			readl(gdrm_disp_base[5] + i + 4),
			readl(gdrm_disp_base[5] + i + 8),
			readl(gdrm_disp_base[5] + i + 12));

		/* DSI0 */
		for (i = 0; i < 0x200; i += 16)
			DRM_INFO("DSI   0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[6] + i,
			readl(gdrm_disp_base[6] + i),
			readl(gdrm_disp_base[6] + i + 4),
			readl(gdrm_disp_base[6] + i + 8),
			readl(gdrm_disp_base[6] + i + 12));

		/* MUTEX */
		for (i = 0; i < 0x100; i += 16)
			DRM_INFO("MUTEX 0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[7] + i,
			readl(gdrm_disp_base[7] + i),
			readl(gdrm_disp_base[7] + i + 4),
			readl(gdrm_disp_base[7] + i + 8),
			readl(gdrm_disp_base[7] + i + 12));

		/* OD */
		for (i = 0; i < 0x100; i += 16)
			DRM_INFO("OD    0x%08X: %08X %08X %08X %08X\n",
			gdrm_disp_table[8] + i,
			readl(gdrm_disp_base[8] + i),
			readl(gdrm_disp_base[8] + i + 4),
			readl(gdrm_disp_base[8] + i + 8),
			readl(gdrm_disp_base[8] + i + 12));
	} else if (0 == strncmp(opt, "hdmi:", 5)) {
		int i;

		/* CONFIG */
		for (i = 0; i < 0x120; i += 16)
			DRM_INFO("CFG    0x%08X: %08X %08X %08X %08X\n",
			gdrm_hdmi_table[0] + i,
			readl(gdrm_hdmi_base[0] + i),
			readl(gdrm_hdmi_base[0] + i + 4),
			readl(gdrm_hdmi_base[0] + i + 8),
			readl(gdrm_hdmi_base[0] + i + 12));

		/* OVL1 */
		for (i = 0; i < 0x260; i += 16)
			DRM_INFO("OVL1   0x%08X: %08X %08X %08X %08X\n",
			gdrm_hdmi_table[1] + i,
			readl(gdrm_hdmi_base[1] + i),
			readl(gdrm_hdmi_base[1] + i + 4),
			readl(gdrm_hdmi_base[1] + i + 8),
			readl(gdrm_hdmi_base[1] + i + 12));
		for (i = 0xf40; i < 0xfc0; i += 16)
			DRM_INFO("OVL1   0x%08X: %08X %08X %08X %08X\n",
			gdrm_hdmi_table[1] + i,
			readl(gdrm_hdmi_base[1] + i),
			readl(gdrm_hdmi_base[1] + i + 4),
			readl(gdrm_hdmi_base[1] + i + 8),
			readl(gdrm_hdmi_base[1] + i + 12));

		/* RDMA1 */
		for (i = 0; i < 0x100; i += 16)
			DRM_INFO("RDMA1  0x%08X: %08X %08X %08X %08X\n",
			gdrm_hdmi_table[2] + i,
			readl(gdrm_hdmi_base[2] + i),
			readl(gdrm_hdmi_base[2] + i + 4),
			readl(gdrm_hdmi_base[2] + i + 8),
			readl(gdrm_hdmi_base[2] + i + 12));

		/* COLOR1 */
		for (i = 0x400; i < 0x500; i += 16)
			DRM_INFO("COLOR1 0x%08X: %08X %08X %08X %08X\n",
			gdrm_hdmi_table[3] + i,
			readl(gdrm_hdmi_base[3] + i),
			readl(gdrm_hdmi_base[3] + i + 4),
			readl(gdrm_hdmi_base[3] + i + 8),
			readl(gdrm_hdmi_base[3] + i + 12));
		for (i = 0xC00; i < 0xD00; i += 16)
			DRM_INFO("COLOR1 0x%08X: %08X %08X %08X %08X\n",
			gdrm_hdmi_table[3] + i,
			readl(gdrm_hdmi_base[3] + i),
			readl(gdrm_hdmi_base[3] + i + 4),
			readl(gdrm_hdmi_base[3] + i + 8),
			readl(gdrm_hdmi_base[3] + i + 12));

		/* GAMMA */
		for (i = 0; i < 0x100; i += 16)
			DRM_INFO("GAMMA  0x%08X: %08X %08X %08X %08X\n",
			gdrm_hdmi_table[4] + i,
			readl(gdrm_hdmi_base[4] + i),
			readl(gdrm_hdmi_base[4] + i + 4),
			readl(gdrm_hdmi_base[4] + i + 8),
			readl(gdrm_hdmi_base[4] + i + 12));
	} else {
	    goto Error;
	}

	return;
 Error:
	DRM_ERROR("Parse command error!\n\n%s", STR_HELP);
}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	DRM_INFO("[mtkdrm_dbg] %s\n", cmd);
	memset(debug_buffer, 0, sizeof(debug_buffer));
	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

/* ------------------------------------------------------------------------- */
/* Debug FileSystem Routines */
/* ------------------------------------------------------------------------- */
static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count,
	loff_t *ppos)
{
	if (strlen(debug_buffer))
		return simple_read_from_buffer(ubuf, count, ppos, debug_buffer,
			strlen(debug_buffer));
	else
		return simple_read_from_buffer(ubuf, count, ppos, STR_HELP,
			strlen(STR_HELP));
}

static char dis_cmd_buf[512];
static ssize_t debug_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(dis_cmd_buf) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&dis_cmd_buf, ubuf, count))
		return -EFAULT;

	dis_cmd_buf[count] = 0;

	process_dbg_cmd(dis_cmd_buf);

	return ret;
}

struct dentry *mtkdrm_dbgfs = NULL;
static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

void mediatek_drm_debugfs_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	DRM_INFO("mediatek_drm_debugfs_init\n");
	mtkdrm_dbgfs = debugfs_create_file("mtkdrm", S_IFREG | S_IRUGO |
			S_IWUSR | S_IWGRP, NULL, (void *)0, &debug_fops);

	gdrm_disp_base[0] = mtk_crtc->regs;
	gdrm_disp_base[1] = mtk_crtc->ovl_regs[0];
	gdrm_disp_base[2] = mtk_crtc->rdma_regs[0];
	gdrm_disp_base[3] = mtk_crtc->color_regs[0];
	gdrm_disp_base[4] = mtk_crtc->aal_regs;
	gdrm_disp_base[5] = mtk_crtc->ufoe_regs;
	gdrm_disp_base[6] = mtk_crtc->dsi_reg;
	gdrm_disp_base[7] = mtk_crtc->mutex_regs;
	gdrm_disp_base[8] = mtk_crtc->od_regs;

	gdrm_hdmi_base[0] = mtk_crtc->regs;
	gdrm_hdmi_base[1] = mtk_crtc->ovl_regs[1];
	gdrm_hdmi_base[2] = mtk_crtc->rdma_regs[1];
	gdrm_hdmi_base[3] = mtk_crtc->color_regs[1];
	gdrm_hdmi_base[4] = mtk_crtc->gamma_regs;

}

void mediatek_drm_debugfs_deinit(void)
{
	debugfs_remove(mtkdrm_dbgfs);
}

