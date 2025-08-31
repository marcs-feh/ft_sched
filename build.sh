#!/usr/bin/env sh

cc='clang++'
cflags='-std=c++20 -fno-strict-aliasing -fwrapv -fno-exceptions -fno-rtti'
wflags='-Wall -Wextra -Werror=return-type -D_CRT_SECURE_NO_WARNINGS'

Run(){ echo "-> $@"; $@; }

set -eu

cflags="$cflags $wflags"

echo [Code generation]
Run $cc $cflags $wflags generate.cpp base.cpp -o generate.exe
./generate.exe

echo [Compile]
Run $cc $cflags $wflags main.cpp base.cpp -o ft_sched.exe
./ft_sched.exe
