# drm/tegra depends on host1x, so if both drivers are built-in care must be
# taken to initialize them in the correct order. Link order is the only way
# to ensure this currently.
KBUILD_CFLAGS += -O3 -fdata-sections -ffunction-sections -finline-functions -ftree-vectorize -funroll-loops

obj-$(CONFIG_TEGRA_HOST1X)	+= host1x/
obj-y			+= drm/ vga/ arm/
obj-$(CONFIG_IMX_IPUV3_CORE)	+= ipu-v3/
