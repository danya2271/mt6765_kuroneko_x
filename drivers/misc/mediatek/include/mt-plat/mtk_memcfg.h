/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __MTK_MEMCFG_H__
#define __MTK_MEMCFG_H__
#include <linux/fs.h>

/* late warning flags */
#define WARN_MEMBLOCK_CONFLICT	(1 << 0)	/* memblock overlap */
#define WARN_MEMSIZE_CONFLICT	(1 << 1)	/* dram info missing */
#define WARN_API_NOT_INIT	(1 << 2)	/* API is not initialized */

#define MTK_MEMCFG_MEMBLOCK_PHY 0x1

#define MTK_MEMCFG_LOG_AND_PRINTK(fmt, arg...)

#define mtk_memcfg_record_freed_reserved(start, end) do {} while (0)
#define mtk_memcfg_inform_vmpressure() do { } while (0)
#endif /* end __MTK_MEMCFG_H__ */
