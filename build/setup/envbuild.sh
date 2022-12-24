#!/bin/bash

abort() { echo "$@"; exit 1; }

source build/functions.sh

check_argument "$1" "Pass valid device name."

export KURONEKO_TYPE="$2"
export NAME="kuroneko_x"

case $2 in

  "stable")
    echo "Building Kuroneko Stable."
    ;;

  "balance")
    echo "Building Kuroneko Balance."
    ;;

  "gaming")
    echo "Building Kuroneko Gaming."
    ;;

  "extreme")
    echo "Building Kuroneko Extreme."
    ;;

  "powersave")
    echo "Building Kuroneko Powersave."
    ;;

  *)
    export KURONEKO_TYPE="stable";
    echo_if_argument "$2" "NOTE: Invalid Kuroneko type passed." "NOTE: Release wasn't selected. Falling back to defaults.";
    echo "NOTE: Auto-selected stable release.";
    echo "NOTE: Avaliable types: stable, balance, gaming, extreme, powersave";
    echo "Building Kuroneko Stable";
    ;;
esac

export BUILDROOT=$(pwd)
export SOURCEROOT="$BUILDROOT/.."
export DEVICE="$1"

check_file "${BUILDROOT}/build/setup/devices/${DEVICE}.sh" "Device config not found. Abort.";

source ${BUILDROOT}/build/setup/devices/$DEVICE.sh

source ${BUILDROOT}/build/setup/buildenv.sh

source ${BUILDROOT}/build/setup/build-image.sh

source ${BUILDROOT}/build/pack/ota-pack.sh $BUILDROOT/out/$KERNEL_IMAGE
