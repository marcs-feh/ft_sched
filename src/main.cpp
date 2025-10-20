#include <stdio.h>
#include <math.h>

#include "base.hpp"

#include "ft_sched.hpp"
#include "task.hpp"

template<class T>
void print_list(List<T> const& list, char const* elem_fmt){
	printf("len: %td cap: %td [ ", list.len, list.cap);
	for(usize i = 0; i < list.len; i ++){
		printf(elem_fmt, list[i]);
		printf(" ");
	} printf("]\n");
}

template<class T>
void print_slice(Slice<T> slice, char const* elem_fmt){
	printf("len: %td [ ", slice.len);
	for(usize i = 0; i < slice.len; i ++){
		printf(elem_fmt, slice[i]);
		printf(" ");
	} printf("]\n");
}

static
i32 randint(i32 a, i32 b){
    return (rand() % (b - a + 1)) + a;
}

TimeTick start = 0;


// init_spsc_queue()

struct Deadline {
	TimeTick last_tick{0};
	TimeTick deadline{0};
	RawTask* task;

	// bool check(){
};


Arena permanent_arena;
u8 permanent_arena_data[4 * mem_kilobyte];

//// Software watchdog timer
Atomic<TimeTick> watchdog_last_tick = 0;

void watchdog_timer_func(RawTask*){
	const Duration limit = Duration::from_milli(500);
	watchdog_last_tick = tick_now();

	while(1){
		auto now = tick_now();
		auto elapsed = tick_diff(now, watchdog_last_tick.load(memory_order_relaxed));

		if(elapsed > limit){
			fprintf(stderr, "[WD] Failed! limit=%tdms elapsed=%tdms\n", limit.to_milli(), elapsed.to_milli());
			panic("Watchdog fail.");
		}
		sleep_for(Duration::from_second(0));
	}
}

void reset_watchdog(){
	watchdog_last_tick.store(tick_now(), memory_order_relaxed);
}
////-----------------------

static
void print_info(){
	printf("Address Width:  %zu-bit\n", sizeof(void*) * 8);
	printf("Tick Frequency: %tu Hz\n", tick_frequency());
}

void somebody(RawTask* t){
	sleep_for(Duration::from_milli(480));
	reset_watchdog();
	sleep_for(Duration::from_milli(450));
	printf("Hello from task %p, it's been: %td us\n", t, tick_diff(tick_now(), start).to_micro());
}

template<typename T>
struct PoolSlot {
	union {
		T data;
		PoolSlot<T>* next;
	};
};

template<typename T>
struct Pool {
	PoolSlot<T>* slot;
	u16 first_free;
};


// template<class Output>
// struct SimpleTask {
// 	RawTask raw;

// 	void run(){
// 	}

// 	Output result(){
// 	}

// };

// template<typename T>
// struct Option {
// 	union {
// 		T _value;
// 	};
// 	bool _has_value = false;

// 	T get(){
// 		if(!_has_value){
// 			panic("get() on empty option");
// 		}
// 	}

// 	Option() : _has_value{false} {}

// 	Option(T&& v)
// 		: _value{move(v)}
// 		, _has_value{true} {}
	
// 	Option(T const& v){
// 		: _value{v}
// 		, _has_value{true} {}
// 	}

// 	Option(Option<T>&& other)
// 		, _has_value{exchange(other._has_value, false)}
// 	{
// 		if(_has_value){
// 			new (&_value) 
// 		}
// 	}
	

// 	Option<T>* drop(){
// 		if(_has_value){
// 			_value.~T();
// 		}
// 		_has_value = false;
// 		return this;
// 	}

// 	~Option(){
// 		drop();
// 	}

// };


int main(){
	srand(tick_now());
	permanent_arena = arena_from_buffer(Slice<u8>{&permanent_arena_data[0], sizeof(permanent_arena_data)});

	start = tick_now();
	usize arena_size = 24 * mem_kilobyte;
	u8* arena_data = new u8[arena_size];

	Arena arena = arena_from_buffer(Slice<u8>{&arena_data[0], arena_size});
	print_info();

	auto wd = make_raw_task(&arena, watchdog_timer_func, nullptr, 0);
	wd->run();

	auto t = make_tmr_task(&arena, 2 * mem_kilobyte, somebody, nullptr, 0);
	t->run();

	t->join();
}
