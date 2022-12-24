#!/bin/bash

export NAME="kuroneko_x"

if !([[ -n $BUILDROOT ]]); then
  export BUILDROOT=$(pwd)
fi

if !([[ -n $KURONEKO_TYPE ]]); then
  export KURONEKO_TYPE="stable"
fi

source $BUILDROOT/build/functions.sh

check_file "$1" "File not found."

if !([[ -n $KVERSION ]]); then

  export KVERSION=$(dialog --title "Kuroneko OTA" --clear --inputbox "Version: " 8 20 --output-fd 1)

  case $KVERSION in
      ''|*[!0-9]*) abort "Invalid input" ;;
      *);;
  esac

fi;

echo "Packing into AnyKernel3..."

export WORK=$BUILDROOT/work

mkdir $WORK
cp $BUILDROOT/build/external/AnyKernel3 $WORK/ak3 -r

cp "$1" $WORK/ak3

cd $WORK/ak3/
zip -r9 "${NAME}_build_${KVERSION}[${KURONEKO_TYPE^^}].zip" * -x .git README.md *placeholder
mv "${NAME}_build_${KVERSION}[${KURONEKO_TYPE^^}].zip" $BUILDROOT/
cd $BUILDROOT

rm -rf $WORK

echo "-----"

#$BUILDROOT/build/pack/sign-ota.sh "${NAME}_build_${KVERSION}[${KURONEKO_TYPE^^}].zip"