#!/bin/bash

if [ -n $BUILDROOT ]; then
  export BUILDROOT=$(pwd)
fi

if [ -n $KURONEKO_TYPE ]; then
  export KURONEKO_TYPE="stable"
fi

source $BUILDROOT/build/functions.sh

check_file "$1" "File not found."

mkdir secured
java -Djava.library.path="$BUILDROOT/build/external/jar/conscrypt-android-2.5.2.jar" -jar $BUILDROOT/build/external/signapk.jar $BUILDROOT/build/pack/secure/certificate.pem $BUILDROOT/build/pack/secure/key.pk8 $1 secured/$1
