# Makefile for Linux samples code
KBUILD_CFLAGS += -mcpu=cortex-a53+crc -mtune=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard

obj-$(CONFIG_SAMPLES)	+= kobject/ kprobes/ trace_events/ livepatch/ \
			   hw_breakpoint/ kfifo/ kdb/ hidraw/ rpmsg/ seccomp/ \
			   configfs/ connector/ v4l/ trace_printk/ blackfin/
