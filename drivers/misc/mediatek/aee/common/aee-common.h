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


#ifdef pr_info
#undef pr_info
#endif

#ifdef pr_debug
#undef pr_debug
#endif

#ifdef pr_notice
#undef pr_notice
#endif

#define pr_info(format, args...)
#define pr_debug(format, args...)
#define pr_notice(format, args...)

#if !defined(AEE_COMMON_H)
#define AEE_COMMON_H
#include <linux/console.h>

extern int aee_rr_reboot_reason_show(struct seq_file *m, void *v);
extern int aee_rr_last_fiq_step(void);
extern void aee_rr_rec_exp_type(unsigned int type);

extern int debug_locks;
#ifdef CONFIG_SMP
extern void irq_raise_softirq(const struct cpumask *mask, unsigned int irq);
#endif

/* for test case only */
extern int no_zap_locks;

#endif				/* AEE_COMMON_H */
