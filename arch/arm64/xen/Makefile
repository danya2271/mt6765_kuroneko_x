KBUILD_CFLAGS += -mcpu=cortex-a53 -Wa,-mcpu=cortex-a53 -mtune=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard 

xen-arm-y	+= $(addprefix ../../arm/xen/, enlighten.o grant-table.o p2m.o mm.o)
obj-y		:= xen-arm.o hypercall.o
obj-$(CONFIG_XEN_EFI) += $(addprefix ../../arm/xen/, efi.o)
