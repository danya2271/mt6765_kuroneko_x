#!/bin/bash
# This file is done RIGHT for this kernel.


source $BUILDROOT/build/functions.sh

sync_gcc() {
  cd $SOURCEROOT

  wget https://releases.linaro.org/components/toolchain/binaries/latest-5/aarch64-linux-gnu/gcc-linaro-5.5.0-2017.10-x86_64_aarch64-linux-gnu.tar.xz && tar -xf gcc-linaro-5.5.0-2017.10-x86_64_aarch64-linux-gnu.tar.xz
  mv gcc-linaro-5.5.0-2017.10-x86_64_aarch64-linux-gnu gcc
  rm gcc-linaro-5.5.0-2017.10-x86_64_aarch64-linux-gnu.tar.xz

  cd $BUILDROOT
}

sync_clang() {
  cd $SOURCEROOT

  git clone https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/ -b android10-gsi --depth 1 --no-tags --single-branch clang_all && mv clang_all/clang-r353983c clang
  rm -rf clang_all

  cd $BUILDROOT
}

echo "-----"
echo "Detecting environment..."

if [[ -n $DEVICE_CLANG ]];
then
  export KERNEL_CLANG_PATH="$SOURCEROOT/clang"
  export KERNEL_CLANG="clang"

  [[ -d $KERNEL_CLANG_PATH ]] && echo "Found clang."
  if !([[ -d $KERNEL_CLANG_PATH ]]); then
    sync_clang
  fi

  export PATH="$KERNEL_CLANG_PATH/bin:$PATH"

  CLANG_VERSION=$(${KERNEL_CLANG} --version | grep version | sed "s|clang version ||")
fi

export KERNEL_CCOMPILE64_PATH="$SOURCEROOT/gcc"
export KERNEL_CCOMPILE64="aarch64-linux-gnu-"

[[ -d $KERNEL_CCOMPILE64_PATH ]] && echo "Found GCC."
if !([[ -d $KERNEL_CCOMPILE64_PATH ]]); then
  sync_gcc
fi

export PATH="$KERNEL_CCOMPILE64_PATH/bin:$PATH"

GCC_VERSION=$(${KERNEL_CCOMPILE64}gcc --version | grep "(GCC)" | sed 's|.*) ||')

echo "Building with:"

if [[ -n $DEVICE_CLANG ]];
then
  echo "    - clang $CLANG_VERSION"
fi
echo "    - GCC $GCC_VERSION"
echo "-----"

#make CC=$KERNEL_CLANG CROSS_COMPILE=$KERNEL_CCOMPILE64 O=out ARCH=$DEVICE_ARCH -j4 $1 $2 $3 $4 $5

#make CC=/home/danya227/clang_compiler/bin/clang-9 CROSS_COMPILE=/home/danya227/Gcc/bin/aarch64-linux-gnu- O=out ARCH=arm64 -j4 $1 $2 $3 $4 $5
