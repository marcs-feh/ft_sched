#!/usr/bin/env sh

cc=clang++
cflags='-std=c++14 -fno-strict-aliasing -fwrapv -O0'
wflags='-Wall -Wextra -Werror=return-type'

Run(){ echo "$@"; $@; }

set -eu

cflags="$cflags $wflags"

Run $cc $cflags $wflags main.cpp -o ft_sched.exe
