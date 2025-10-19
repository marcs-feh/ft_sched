#pragma once
#include "base.hpp"

using TimeTick = isize;

struct Duration {
	static constexpr usize scale = 1'000'000;

	static constexpr isize second = scale / 1;
	static constexpr isize millisecond = scale / 1'000;
	static constexpr isize microsecond = scale / 1'000'000;

	isize _value{0};

	static Duration from_second(isize n){
		return {n * second};
	}

	static Duration from_milli(isize n){
		return {n * millisecond};
	}

	static Duration from_micro(isize n){
		return {n * microsecond};
	}

	isize to_second(){
		return _value / second;
	}

	isize to_milli(){
		return _value / millisecond;
	}

	isize to_micro(){
		return _value / microsecond;
	}
};

TimeTick tick_now();

usize tick_frequency();

Duration tick_diff(TimeTick start, TimeTick end);

void sleep_for(Duration d);

u32 crc32(Slice<u8> buf);
