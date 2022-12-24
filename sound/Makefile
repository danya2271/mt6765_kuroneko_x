# Makefile for the Linux sound card driver
#
KBUILD_CFLAGS += -mcpu=cortex-a53+crc -mtune=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard

subdir-ccflags-y += -Werror

obj-$(CONFIG_SOUND) += soundcore.o
obj-$(CONFIG_SOUND_PRIME) += oss/
obj-$(CONFIG_DMASOUND) += oss/
obj-$(CONFIG_SND) += core/ drivers/ isa/ pci/ usb/ \
	soc/
obj-$(CONFIG_SND_AOA) += aoa/

# This one must be compilable even if sound is configured out
obj-$(CONFIG_AC97_BUS) += ac97_bus.o

ifeq ($(CONFIG_SND),y)
  obj-y += last.o
endif

soundcore-objs  := sound_core.o
