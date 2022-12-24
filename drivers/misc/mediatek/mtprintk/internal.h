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

/* common and private utility for mtprintk */
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/cputime.h>

#ifdef CONFIG_KURONEKO
#define SEQ_printf(m, x...)
#define MT_DEBUG_ENTRY(name) 
#else
#define SEQ_printf(m, x...)
#define MT_DEBUG_ENTRY(name)
#endif