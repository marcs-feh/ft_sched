#pragma once
#include "base.hpp"

using TimeTick = isize;

struct Duration {
	static constexpr usize scale = 1'000'000; // Microsec
	isize _value{0};
};

TimeTick tick_now();

usize tick_frequency();

Duration tick_diff(TimeTick start, TimeTick end);

void sleep_for(Duration d);

u32 crc32(Slice<u8> buf);
