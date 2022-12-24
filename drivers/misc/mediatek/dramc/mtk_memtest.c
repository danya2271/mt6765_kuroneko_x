/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
/* #include <mach/mtk_clkmgr.h> */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/highmem.h>
#include <asm/setup.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_meminfo.h>
#include <mt-plat/mtk_chip.h>
#include <mt-plat/aee.h>

#include "mtk_dramc.h"
#include "dramc.h"

#ifdef CONFIG_OF_RESERVED_MEM
#define DRAM_R0_MEMTEST_RESERVED_KEY "reserve-memory-dram_r0_memtest"
#define DRAM_R1_MEMTEST_RESERVED_KEY "reserve-memory-dram_r1_memtest"
#include <linux/of_reserved_mem.h>
#include <mt-plat/mtk_memcfg.h>
#endif