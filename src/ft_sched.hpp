#pragma once
#include "base.hpp"

using TimeTick = isize;

struct Duration {
	isize _nsec{0};
};

TimeTick tick_now();

usize tick_frequency();

Duration tick_diff(TimeTick start, TimeTick end);

u32 crc32(Slice<u8> buf);
