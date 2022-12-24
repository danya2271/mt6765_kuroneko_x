#
# Makefile for the kernel mmc device drivers.
#
KBUILD_CFLAGS += -Wno-unused-variable

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-$(CONFIG_MMC_DEBUG) := -DDEBUG

obj-$(CONFIG_MMC)		+= core/
obj-$(CONFIG_MMC)		+= card/
obj-$(subst m,y,$(CONFIG_MMC))	+= host/
