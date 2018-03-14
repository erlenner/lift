#!/bin/bash
cd $(dirname "$0")/..

until ./build-native/src/lift/run "${@:1}"; do
    echo "crashed with code $?. Respawning" >&2
    sleep 1
done
