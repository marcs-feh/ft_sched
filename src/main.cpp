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
	for(int i = 0; i < 10'000'000; i++);
	printf("Hello from task %p, it's been: %tdus\n", t, tick_diff(tick_now(), start)._nsec / 1'000);
}

// init_spsc_queue()


struct Deadline {
	TimeTick last_tick{0};
	TimeTick deadline{0};
	RawTask* task;

	// bool check(){}
};


int main(){
	start = tick_now();
	usize arena_size = 24 * mem_kilobyte;
	u8* arena_data = new u8[arena_size];

	Arena arena = arena_from_buffer({arena_data, arena_size});
	printf("Sizeof RawTask: %tu\n", sizeof(RawTask));

	f64 nano_per_tick = 1.0e9 / f64(tick_frequency());
	printf("Frequency: %tu\n", tick_frequency());
	printf("ns/tick: %g\n", nano_per_tick);

	auto t = make_tmr_task(&arena, 2 * mem_kilobyte, somebody, nullptr, 0);
	t->run();

	t->join();

	// for(usize i = 0; i < 20; i++){
	// 	auto task = make_raw_task(&arena, somebody, nullptr, 0);
	// 	runnables.append(task);
	// }

	// for(usize i = 0; i < runnables.len; i++){
	// 	runnables[i]->run();
	// }

	// for(usize i = 0; i < runnables.len; i++){
	// 	runnables[i]->join();
	// }
}

