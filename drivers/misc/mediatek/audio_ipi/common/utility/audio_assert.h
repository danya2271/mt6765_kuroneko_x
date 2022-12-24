/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef AUDIO_ASSERT_H
#define AUDIO_ASSERT_H

#define AIPI_NOLOG 1

#if AIPI_NOLOG

#ifdef pr_info
#undef pr_info
#endif

#ifdef pr_debug
#undef pr_debug
#endif

#ifdef pr_notice
#undef pr_notice
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_info(format, args...)
#define pr_debug(format, args...)
#define pr_notice(format, args...)
#define pr_fmt(fmt)

#endif

#define AUD_ASSERT(exp)

#define AUD_WARNING(string)



#endif /* end of AUDIO_ASSERT_H */

