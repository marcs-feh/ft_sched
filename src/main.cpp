#include <stdio.h>

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

// template<TaskBody Fn> [[nodiscard]]
// Executable* make_task(Arena* a, Fn body){
// 	auto t = make<Task<Fn>>(a, body);
// 	return t;
// }

TimeTick start = 0;

void somebody(RawTask* t){
	for(i64 i = 0; i < 8'000'000'000; i++){
	}

	printf("Hello from task %p, it's been: %td us\n", t, tick_diff(tick_now(), start)._value);
}

// init_spsc_queue()


struct Deadline {
	TimeTick last_tick{0};
	TimeTick deadline{0};
	RawTask* task;

	// bool check(){
};


Arena permanent_arena;
u8 permanent_arena_data[4 * mem_kilobyte];

// usize wd_timer_ns = 0;
// void watchdog_timer(Task* t){
// 	wd_timer_ns();
// }

// void spawn_watchdog_timer(){
// 	make_raw_task(&permanent_arena);
// }

static
void print_info(){

	f64 tick_duration = f64(Duration::scale) / f64(tick_frequency());

	cstring scale_suffix = "?";
	switch(Duration::scale){
	case 1: scale_suffix = "s"; break;
	case 1'000: scale_suffix = "ms"; break;
	case 1'000'000: scale_suffix = "us"; break;
	case 1'000'000'000: scale_suffix = "ns"; break;
	default: panic("Invalid duration"); break;
	}
	
	printf("Address Width:  %zu-bit\n", sizeof(void*) * 8);
	printf("Tick Frequency: %tu Hz\n", tick_frequency());
	printf("Tick Duration:  %g %s\n", tick_duration, scale_suffix);
}

int main(){
	permanent_arena = arena_from_buffer(Slice<u8>{&permanent_arena_data[0], sizeof(permanent_arena_data)});

	start = tick_now();
	usize arena_size = 24 * mem_kilobyte;
	u8* arena_data = new u8[arena_size];

	Arena arena = arena_from_buffer(Slice<u8>{&arena_data[0], arena_size});
	print_info();


	auto t = make_tmr_task(&arena, 2 * mem_kilobyte, somebody, nullptr, 0);
	t->run();

	t->join();
}

