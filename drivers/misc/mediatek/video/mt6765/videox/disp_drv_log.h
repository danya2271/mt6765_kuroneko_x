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

#ifndef __DISP_DRV_LOG_H__
#define __DISP_DRV_LOG_H__

#include "display_recorder.h"
#include "ddp_debug.h"

#define DISP_LOG_PRINT(level, sub_module, fmt, args...)
#define DISPINFO(string, args...)
#define DISPMSG(string, args...)
#define DISPCHECK(string, args...)
#define DISPWARN(string, args...)
#define DISPERR(string, args...)
#define DISPPR_FENCE(string, args...)
#define DISPDBG(string, args...)
#define DISPFUNC()
#define DISPDBGFUNC() DISPFUNC()

#define DISPPR_HWOP(string, args...)

#define disp_aee_print(string, args...) 
#define disp_aee_db_print(string, args...)

#define _DISP_PRINT_FENCE_OR_ERR(is_err, string, args...) 
#endif /* __DISP_DRV_LOG_H__ */
