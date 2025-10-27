#pragma once
#include "base.hpp"

using TimeTick = isize;

struct Duration {
	static constexpr isize scale = 1'000'000; // 10MHz

	static constexpr isize second = scale / 1;
	static constexpr isize millisecond = scale / 1'000;
	static constexpr isize microsecond = scale / 1'000'000;

	i64 _value{0};

	constexpr static Duration from_second(isize n){
		return {(n * scale) / 1};
	}

	constexpr static Duration from_milli(isize n){
		isize val = {(n * scale) / 1'000};
		return Duration{val};
	}

	constexpr static Duration from_micro(isize n){
		return {(n * scale) / 1'000'000};
	}

	constexpr isize to_second() const {
		return _value / second;
	}

	constexpr isize to_milli() const {
		return _value / millisecond;
	}

	constexpr isize to_micro() const {
		return _value / microsecond;
	}

	#define X(Op) constexpr Duration operator Op(Duration d) const { return { _value Op d._value }; }
		X(+) X(-) X(*) X(/)
	#undef X

	#define X(Op) constexpr bool operator Op(Duration d) const { return this->_value Op d._value; }
		X(<) X(>) X(<=) X(>=) X(==) X(!=)
	#undef X
};

TimeTick tick_now();

usize tick_frequency();

// static inline
// Duration tick_diff(TimeTick a, TimeTick b){
// 	isize diff = a - b;
// 	return {diff / isize(tick_frequency() / Duration::scale)};
// }

static inline
Duration tick_diff(TimeTick a, TimeTick b){
	i64 diff = i64(a) - i64(b);
	auto time_diff = static_cast<isize>((diff * i64(Duration::scale)) / i64(tick_frequency()));
	return {time_diff};
}

void sleep_for(Duration d);

u32 crc32(Slice<u8> buf);
