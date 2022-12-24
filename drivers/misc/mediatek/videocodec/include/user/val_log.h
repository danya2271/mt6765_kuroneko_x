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

#ifndef _VAL_LOG_H_
#define _VAL_LOG_H_

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "MFV_COMMON" /* /< LOG_TAG "MFV_COMMON" */
#include <cutils/xlog.h>
#include <utils/Log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODULE_MFV_LOGE(...) /* /< NOT show error log */
#define VDO_LOGE(...)	/* /< NOT show error log */
#define MODULE_MFV_LOGW(...) /* /< NOT show warning log */
#define VDO_LOGW(...)	/* /< NOT show warning log */
#define MODULE_MFV_LOGD(...) /* /< NOT show debug information log */
#define VDO_LOGD(...)	/* /< NOT show debug information log */
#define MODULE_MFV_LOGI(...) /* /< NOT show information log */
#define VDO_LOGI(...)	/* /< NOT show information log */

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _VAL_LOG_H_ */
