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

#include <linux/bug.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <mt-plat/aee.h>
#include <linux/kgdb.h>
#include <linux/kdb.h>
#include <linux/utsname.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include "aee-common.h"
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <mt-plat/mrdump.h>
#include <mrdump_private.h>

static struct aee_kernel_api *g_aee_api;
#define KERNEL_REPORT_LENGTH 90

#ifdef CONFIG_KGDB_KDB
/* Press key to enter kdb */
void aee_trigger_kdb(void)
{
//	pr_info("User trigger KDB\n");
	/* mtk_set_kgdboc_var(); */
	kgdb_breakpoint();

//	pr_info("Exit KDB\n");
}
#else
/* For user mode or the case KDB is not enabled, print basic debug messages */
void aee_dumpbasic(void)
{
}

void aee_trigger_kdb(void)
{
//	pr_info("\nKDB is not enabled ! Dump basic debug info...\n\n");
//	aee_dumpbasic();
}
#endif

struct aee_oops *aee_oops_create(enum AE_DEFECT_ATTR attr,
		enum AE_EXP_CLASS clazz, const char *module)
{
/*	struct aee_oops *oops = kzalloc(sizeof(struct aee_oops), GFP_ATOMIC);

	if (oops == NULL)
		return NULL;
	oops->attr = attr;
	oops->clazz = clazz;
	if (module != NULL)
		strlcpy(oops->module, module, sizeof(oops->module));
	else
		strlcpy(oops->module, "N/A", sizeof(oops->module));
	strlcpy(oops->backtrace, "N/A", sizeof(oops->backtrace));
	strlcpy(oops->process_path, "N/A", sizeof(oops->process_path));

	return oops;*/
}
EXPORT_SYMBOL(aee_oops_create);

void aee_oops_free(struct aee_oops *oops)
{
/*	kfree(oops->console);
	kfree(oops->android_main);
	kfree(oops->android_radio);
	kfree(oops->android_system);
	kfree(oops->userspace_info);
	kfree(oops->mmprofile);
	kfree(oops->mini_rdump);
	vfree(oops->userthread_stack.Userthread_Stack);
	vfree(oops->userthread_maps.Userthread_maps);
	kfree(oops);
	pr_notice("%s\n", __func__);*/
}
EXPORT_SYMBOL(aee_oops_free);

void aee_register_api(struct aee_kernel_api *aee_api)
{
	g_aee_api = aee_api;
}
EXPORT_SYMBOL(aee_register_api);

void aee_disable_api(void)
{
	if (g_aee_api) {
		pr_info("disable aee kernel api");
		g_aee_api = NULL;
	}
}
EXPORT_SYMBOL(aee_disable_api);

void aee_kernel_exception_api(const char *file, const int line,
		const int db_opt, const char *module, const char *msg, ...)
{
/*	char msgbuf[KERNEL_REPORT_LENGTH];
	int offset = 0;
	va_list args;

	va_start(args, msg);
	offset += snprintf(msgbuf, KERNEL_REPORT_LENGTH, "<%s:%d> ", file,
				line);
	offset += vsnprintf(msgbuf + offset, KERNEL_REPORT_LENGTH - offset, msg,
				args);
	if (g_aee_api && g_aee_api->kernel_reportAPI)
		g_aee_api->kernel_reportAPI(AE_DEFECT_EXCEPTION, db_opt, module,
				msgbuf);
	else
		pr_notice("AEE kernel exception: %s", msgbuf);
	va_end(args);*/
}
EXPORT_SYMBOL(aee_kernel_exception_api);

void aee_kernel_warning_api(const char *file, const int line, const int db_opt,
		const char *module, const char *msg, ...)
{
/*	char msgbuf[KERNEL_REPORT_LENGTH];
	int offset = 0;
	va_list args;

	va_start(args, msg);
	offset += snprintf(msgbuf, KERNEL_REPORT_LENGTH, "<%s:%d> ", file,
			line);
	offset += vsnprintf(msgbuf + offset, KERNEL_REPORT_LENGTH - offset,
			msg, args);

	if (g_aee_api && g_aee_api->kernel_reportAPI) {
		if (module && strstr(module,
			"maybe have other hang_detect KE DB"))
			g_aee_api->kernel_reportAPI(AE_DEFECT_FATAL, db_opt,
				module, msgbuf);
		else
			g_aee_api->kernel_reportAPI(AE_DEFECT_WARNING, db_opt,
				module, msgbuf);
	} else {
		pr_notice("AEE kernel warning: %s", msgbuf);
	}
	va_end(args);*/
}
EXPORT_SYMBOL(aee_kernel_warning_api);

void aee_kernel_reminding_api(const char *file, const int line,
		const int db_opt, const char *module, const char *msg, ...)
{
/*	char msgbuf[KERNEL_REPORT_LENGTH];
	int offset = 0;
	va_list args;

	va_start(args, msg);
	offset += snprintf(msgbuf, KERNEL_REPORT_LENGTH, "<%s:%d> ",
				file, line);
	offset += vsnprintf(msgbuf + offset, KERNEL_REPORT_LENGTH - offset,
				msg, args);
	if (g_aee_api && g_aee_api->kernel_reportAPI)
		g_aee_api->kernel_reportAPI(AE_DEFECT_REMINDING, db_opt,
				module, msgbuf);
	else
		pr_notice("AEE kernel reminding: %s", msgbuf);
	va_end(args);*/
}
EXPORT_SYMBOL(aee_kernel_reminding_api);

void aed_md_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt)
{
}
EXPORT_SYMBOL(aed_md_exception_api);

void aed_md32_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt)
{
}
EXPORT_SYMBOL(aed_md32_exception_api);

void aed_scp_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt)
{
}
EXPORT_SYMBOL(aed_scp_exception_api);


void aed_combo_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt)
{
}
EXPORT_SYMBOL(aed_combo_exception_api);

void aed_common_exception_api(const char *assert_type, const int *log,
			int log_size, const int *phy, int phy_size,
			const char *detail, const int db_opt)
{
}
EXPORT_SYMBOL(aed_common_exception_api);

char sram_printk_buf[256];

void aee_sram_printk(const char *fmt, ...)
{
}
EXPORT_SYMBOL(aee_sram_printk);

static int __init aee_common_init(void)
{
	int ret = 0;
	return ret;
}

static void __exit aee_common_exit(void)
{
}

module_init(aee_common_init);
module_exit(aee_common_exit);
