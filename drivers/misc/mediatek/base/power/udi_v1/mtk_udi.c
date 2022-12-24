/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/*
 * @file    mtk_udi.c
 * @brief   Driver for UDI interface
 *
 */
/* system includes */
#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#else
#include <common.h> /* for printf */
#endif

/* local includes */
#include <sync_write.h>
#include "mtk_udi_internal.h"

/*-----------------------------------------*/
/* Reused code start                       */
/*-----------------------------------------*/

#ifdef __KERNEL__
#define udi_read(addr)			readl(addr)
#define udi_write(addr, val)	mt_reg_sync_writel((val), ((void *)addr))
#endif

/*
 * LOG
 */
#define	UDI_TAG	  "[mt_udi] "
#ifdef __KERNEL__
#ifdef USING_XLOG
#include <linux/xlog.h>
#define udi_info(fmt, args...)
#else
#define udi_info(fmt, args...)	
#endif
#else
#define udi_info(fmt, args...)	
#endif

#ifdef __KERNEL__
/* Device infrastructure */
static int udi_remove(struct platform_device *pdev)
{
	return 0;
}

static int udi_probe(struct platform_device *pdev)
{
	udi_info("UDI Initial.\n");

	return 0;
}

static int udi_suspend(struct platform_device *pdev, pm_message_t state)
{
	udi_info("UDI suspend\n");
	return 0;
}

static int udi_resume(struct platform_device *pdev)
{
	udi_info("UDI resume\n");
	return 0;
}

struct platform_device udi_pdev = {
	.name   = "mt_udi",
	.id     = -1,
};

static struct platform_driver udi_pdrv = {
	.remove     = udi_remove,
	.shutdown   = NULL,
	.probe      = udi_probe,
	.suspend    = udi_suspend,
	.resume     = udi_resume,
	.driver     = {
		.name   = "mt_udi",
	},
};

/*
 * Module driver
 */
static int __init udi_init(void)
{
	int err = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, DEVICE_GPIO);
	if (node == NULL)
		udi_info("error: cannot find node UDI_NODE!\n");

	/* Setup IO addresses and printf */
	udipin_base = of_iomap(node, 0); /* UDI pinmux reg */
	udi_info("udipin_base = 0x%lx.\n", (unsigned long)udipin_base);
	if (udipin_base == NULL)
		udi_info("udi pinmux get some base NULL.\n");


	/* register platform device/driver */
	err = platform_device_register(&udi_pdev);

	if (err != 0) {
		udi_info("fail to register UDI device @ %s()\n", __func__);
		goto out2;
	}

	err = platform_driver_register(&udi_pdrv);
	if (err != 0) {
		udi_info("%s(), UDI driver callback register failed..\n",
			__func__);
		return err;
	}
out2:
	return err;
}

static void __exit udi_exit(void)
{
	udi_info("UDI de-initialization\n");
	platform_driver_unregister(&udi_pdrv);
	platform_device_unregister(&udi_pdev);
}

module_init(udi_init);
module_exit(udi_exit);

MODULE_DESCRIPTION("MediaTek UDI Driver v0.1");
MODULE_LICENSE("GPL");
#endif /* __KERNEL__ */
