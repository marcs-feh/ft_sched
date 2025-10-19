#pragma once
#include "base.hpp"

using TimeTick = isize;

struct Duration {
	static constexpr isize scale = 10'000'000; // 10MHz

	static constexpr isize second = scale / 1;
	static constexpr isize millisecond = scale / 1'000;
	static constexpr isize microsecond = scale / 1'000'000;

	isize _value{0};

	static Duration from_second(isize n){
		return {(n * scale) / 1};
	}

	static Duration from_milli(isize n){
		return {(n * scale) / 1'000};
	}

	static Duration from_micro(isize n){
		return {(n * scale) / 1'000'000};
	}

	isize to_second() const {
		return _value / second;
	}

	isize to_milli() const {
		return _value / millisecond;
	}

	isize to_micro() const {
		return _value / microsecond;
	}

	constexpr Duration operator+(Duration d) const { return {_value + d._value}; }
	constexpr Duration operator-(Duration d) const { return {_value - d._value}; }

	#define X(Op) constexpr bool operator Op(Duration d) const { return this->_value Op d._value; }
		X(<) X(>) X(<=) X(>=) X(==) X(!=)
	#undef X

};

TimeTick tick_now();

usize tick_frequency();

// Duration tick_diff(TimeTick start, TimeTick end);

static inline
Duration tick_diff(TimeTick a, TimeTick b){
	isize diff = a - b;
	return {diff / isize(tick_frequency() / Duration::scale)};
}

void sleep_for(Duration d);

u32 crc32(Slice<u8> buf);
