#!/usr/bin/env sh

cc='clang++'
cflags='-std=c++14 -fno-strict-aliasing -fwrapv -O0'
wflags='-Wall -Wextra -Werror=return-type -D_CRT_SECURE_NO_WARNINGS'

set -eu

cflags="$cflags $wflags"

echo [Code generation]
$cc $cflags $wflags generate.cpp base.cpp -o generate.exe
./generate.exe

echo [Compile]
$cc $cflags $wflags main.cpp base.cpp -o ft_sched.exe
./ft_sched.exe
