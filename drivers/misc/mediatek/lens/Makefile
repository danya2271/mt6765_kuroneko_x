#
# Makefile for misc devices that really don't fit anywhere else.
#

obj-$(CONFIG_MTK_LENS)   += main/
obj-$(CONFIG_MTK_LENS)   += main2/
ifeq ($(CONFIG_MACH_MT6761), y)
	ifeq ($(CONFIG_EXTREME_LOW_RAM), y)
	else
	obj-$(CONFIG_MTK_LENS)   += main3/
	endif
else
ifeq ($(CONFIG_KURONEKO_EXTREME), y)
else
obj-$(CONFIG_MTK_LENS)   += main3/
endif
endif
obj-$(CONFIG_MTK_LENS)   += sub/
ifeq ($(CONFIG_MACH_MT6761), y)
	ifeq ($(CONFIG_EXTREME_LOW_RAM), y)
	else
	obj-$(CONFIG_MTK_LENS)   += sub2/
	endif
else
ifeq ($(CONFIG_KURONEKO_EXTREME), y)
else
obj-$(CONFIG_MTK_LENS)   += sub2/
endif
endif