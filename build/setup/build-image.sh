#!/bin/bash
source $BUILDROOT/build/functions.sh

echo "Building kernel..."

kecho() {
  echo "<-->"
  echo "$1"
  echo "<-->"
}

if [[ -n $KERNEL_MRPROPER ]]; then
  kecho "Calling mrproper"
  $BUILDROOT/build/setup/make.sh mrproper
fi

kecho "Making Kernel Config: $KERNEL_CONFIG"
$BUILDROOT/build/setup/make.sh $KERNEL_CONFIG

kecho "Building Kernel Image..."
$BUILDROOT/build/setup/make.sh $KERNEL_IMAGE

echo "Done!"

kecho "Copying kernel image to out..."
if !([[ -n $KERNEL_IMAGE ]]); then
  export KERNEL_IMAGE="Image.gz"
fi

check_file out/arch/arm64/boot/${KERNEL_IMAGE} "Image not found! Define one in device config!"

cp out/arch/arm64/boot/${KERNEL_IMAGE} out/${KERNEL_IMAGE}

echo "----"
