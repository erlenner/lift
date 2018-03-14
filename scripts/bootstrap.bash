#!/bin/bash
cd $(dirname "$0")
./getg++-6.sh
./getTup.sh

cd ..
tup
if [ ! -z $1 ] && [ $1 -eq $1 ] 2> /dev/null
then
    ./build-native/src/lift/run "${@:1}"
fi
