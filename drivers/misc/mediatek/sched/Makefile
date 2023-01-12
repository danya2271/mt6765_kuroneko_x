#
# Copyright (C) 2017 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#
KBUILD_CFLAGS += -fdata-sections -ffunction-sections -finline-functions -ftree-vectorize -funroll-loops -ffast-math -fforce-addr 

LINUXINCLUDE += -include $(srctree)/kernel/sched/sched.h

# For CPU topology to user space
obj-y += cputopo.o

ccflags-y += -DMET_USER_EVENT_SUPPORT

#
# RQ stats for TLP estimation
#
# For kernel space operation
obj-$(CONFIG_MTK_SCHED_RQAVG_KS) += sched_avg.o

# For CPU topology to user space
obj-y += sched_ctl.o
