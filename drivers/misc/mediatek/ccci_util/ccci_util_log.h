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

#ifndef __CCCI_UTIL_LOG_H__
#define __CCCI_UTIL_LOG_H__

/* #define BRING_UP_LOG_MODE */
#ifndef BRING_UP_LOG_MODE
/* ------------------------------------------------------------------------- */
/* For normal stage log */
/* ------------------------------------------------------------------------- */
/* No MD id message part */
#define CCCI_UTIL_DBG_MSG(fmt, args...)

#define CCCI_UTIL_INF_MSG(fmt, args...)

#define CCCI_UTIL_ERR_MSG(fmt, args...)

/* With MD id message part */
#define CCCI_UTIL_DBG_MSG_WITH_ID(id, fmt, args...)

#define CCCI_UTIL_INF_MSG_WITH_ID(id, fmt, args...)

#define CCCI_UTIL_NOTICE_MSG_WITH_ID(id, fmt, args...)

#define CCCI_UTIL_ERR_MSG_WITH_ID(id, fmt, args...)
#else

/* ------------------------------------------------------------------------- */
/* For bring up stage log */
/* ------------------------------------------------------------------------- */
/* No MD id message part */
#define CCCI_UTIL_DBG_MSG(fmt, args...)
#define CCCI_UTIL_INF_MSG(fmt, args...) 
#define CCCI_UTIL_ERR_MSG(fmt, args...)

/* With MD id message part */
#define CCCI_UTIL_DBG_MSG_WITH_ID(id, fmt, args...) 
#define CCCI_UTIL_INF_MSG_WITH_ID(id, fmt, args...)
#define CCCI_UTIL_NOTICE_MSG_WITH_ID(id, fmt, args...)
#define CCCI_UTIL_ERR_MSG_WITH_ID(id, fmt, args...)


#endif /* end of #ifndef BRING_UP_LOG_MODE */
#endif /*__CCCI_UTIL_LOG_H__ */
