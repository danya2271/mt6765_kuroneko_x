// SPDX-License-Identifier: GPL-2.0
//
// mtk-afe-debug.c  --  Mediatek AFE Debug
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Kai Chieh Chuang <kaichieh.chuang@mediatek.com>

#include <linux/device.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/uaccess.h>

#include <sound/soc.h>

#include "mtk-afe-debug.h"

#include "mtk-base-afe.h"

#define MAX_DEBUG_WRITE_INPUT 256

/* debugfs ops */
int mtk_afe_debugfs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

ssize_t mtk_afe_debugfs_write(struct file *f, const char __user *buf,
			      size_t count, loff_t *offset)
{

	return 0;
}

/* debug function */
void mtk_afe_debug_write_reg(struct file *file, void *arg)
{
}

MODULE_DESCRIPTION("Mediatek AFE Debug");
MODULE_AUTHOR("Kai Chieh Chuang <kaichieh.chuang@mediatek.com>");
MODULE_LICENSE("GPL v2");

