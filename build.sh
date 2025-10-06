#!/usr/bin/env bash

cc='clang++'
cflags='-std=c++20 -fno-exceptions -fno-rtti -fno-strict-aliasing -fwrapv'
wflags='-Wall -Wextra -Werror=return-type -D_CRT_SECURE_NO_WARNINGS -DTARGET_HOSTED_LINUX'

Run(){ echo "-> $@"; $@; }

set -eu

cflags="$cflags $wflags"

echo '[ Code generation ]'
Run $cc $cflags $wflags generate.cpp base.cpp -o generate.exe
./generate.exe

echo '[ Compile ]'
Run $cc $cflags $wflags main.cpp base.cpp ft_sched.cpp -o ft_sched.exe
./ft_sched.exe
