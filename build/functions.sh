#!/bin/bash
# Functions file.
abort() { echo "$@"; exit 1; }

check_argument() {
  if [ -z "$1" ]
    then
      abort "$2"
  fi;
}

echo_if_argument() {
  if [ -z "$1" ]
    then
      echo "$2"
  else
      echo "$3"
  fi;
}

check_file() {
  if !(test -f "$1");
    then
      abort "$2"
  fi;
}

