#!/bin/bash

export KERNEL_CONFIG="akx_${KURONEKO_TYPE}_defconfig"
# export KERNEL_MRPROPER="true"
export KERNEL_IMAGE="Image.gz"
export DEVICE_ARCH="arm64"
export DEVICE_CLANG="true"
