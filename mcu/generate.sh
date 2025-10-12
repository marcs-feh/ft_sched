#!/usr/bin/env sh

cmake -B build -DCMAKE_TOOLCHAIN_FILE=./cmake/gcc-arm-none-eabi.cmake -GNinja

