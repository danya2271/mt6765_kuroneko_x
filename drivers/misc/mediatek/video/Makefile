#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
KBUILD_CFLAGS += -fdata-sections -ffunction-sections -finline-functions -ftree-vectorize -funroll-loops -ffast-math -fforce-addr -O3

################################################################################
# FrameBuffer Driver for Display
#
#
################################################################################
ifneq ($(filter y,$(CONFIG_MACH_MT8127) $(CONFIG_MACH_MT7623) $(CONFIG_MACH_MT8590)),)
CONFIG_MTK_PLATFORM := mt8127
endif

ifneq ($(CONFIG_MTK_LCM), y)
	obj-y += mtdummy/
else
	######################################
	obj-$(CONFIG_MTK_FB) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
	obj-$(CONFIG_MACH_MT6765) += common/
endif

