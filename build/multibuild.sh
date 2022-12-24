#!/bin/bash

export NAME="kuroneko_x"
export BUILDROOT=$(pwd)
export SOURCEROOT="$BUILDROOT/.."
export DEVICE="$1"

source $BUILDROOT/build/functions.sh

check_argument "$1" "No device was selected. Abort."
check_file "${BUILDROOT}/build/setup/devices/${DEVICE}.sh" "Unknown device. Abort.";

echo "Selected device: $DEVICE"

export OUT_KX="${SOURCEROOT}/packages"
if !([[ -d $OUT_KX ]]); then
  mkdir $OUT_KX
fi;

for release in stable balance gaming extreme powersave
do
  echo "Building $release"
  source $BUILDROOT/build/setup/envbuild.sh $DEVICE $release
  mv "${BUILDROOT}/${NAME}*" $OUT_KX/
done

echo "Done."