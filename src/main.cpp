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

struct Unit{};

Arena permanent_arena;
u8 permanent_arena_data[4 * mem_kilobyte];

//// Software watchdog timer
// Atomic<TimeTick> watchdog_last_tick = 0;

// void watchdog_timer_func(RawTask*){
// 	const Duration limit = Duration::from_milli(500);
// 	watchdog_last_tick = tick_now();

// 	while(1){
// 		auto now = tick_now();
// 		auto elapsed = tick_diff(now, watchdog_last_tick.load(memory_order_relaxed));

// 		if(elapsed > limit){
// 			fprintf(stderr, "[WD] Failed! limit=%tdms elapsed=%tdms\n", limit.to_milli(), elapsed.to_milli());
// 			panic("Watchdog fail.");
// 		}
// 		sleep_for(Duration::from_second(0));
// 	}
// }

// void reset_watchdog(){
// 	watchdog_last_tick.store(tick_now(), memory_order_relaxed);
// }
////-----------------------

int puts(char const*);

static
void print_info(){
	u8 bufdata[48];
	auto buf = Slice<u8>(&bufdata[0], sizeof(bufdata));
	String msg;

	msg = buffer_printf(buf, "Address Width:  %zu-bit", sizeof(void*) * 8); puts(msg.data);
	msg = buffer_printf(buf, "Tick Frequency: %tu Hz", tick_frequency()); puts(msg.data);
	msg = buffer_printf(buf, "RawTask size:   %td", sizeof(RawTask)); puts(msg.data);
}

TimeTick start = 0;
void somebody(RawTask* t){
	sleep_for(Duration::from_milli(480));
	// reset_watchdog();
	sleep_for(Duration::from_milli(450));
	printf("Hello from task %p, it's been: %td us\n", t, tick_diff(tick_now(), start).to_micro());
}

struct Deadline {
	TimeTick last_tick{0};
	Duration limit{0};
	RawTask* task{nullptr};
};

Arena main_arena;
constexpr usize main_arena_size = 4096;
u8 main_arena_memory[main_arena_size];

attribute_force_inline static inline
void entrypoint(){
	main_arena = arena_from_buffer({&main_arena_memory[0], main_arena_size});
	print_info();

	auto queue = make_spsc_queue<i32>(&main_arena, 20);

	Duration producer_delay = Duration::from_milli(2);
	auto producer = make_basic_task(&main_arena, [queue, &producer_delay](){
		i32 n = 1;
		while(1){
			queue->push(n);
			++n;
			sleep_for(producer_delay);
		}
		return Unit{};
	});

	producer->run();

	constexpr Duration delay = Duration::from_milli(30);
	i32 prev = 0;	
	for(int i = 0; i < 100; i++){
		auto elem = queue->pop();
		if(elem.ok()){
			auto x = elem.unwrap();
			printf("%d\n", x);
			if(x - 1 != prev){
				printf("--- Drop ---\n");
			}
			prev = x;
		}
		else {
			printf("\n");
		}
		fflush(stdout);
		sleep_for(delay);
	}

	printf("Producer delay %tdms\n", producer_delay.to_milli());
}

//// ---------------------------------------------
#if defined(FT_SCHED_NO_MAIN)
extern "C" void ft_sched_entrypoint()
#else
int main()
#endif
{
	entrypoint();
}

#include "base.cpp"
#include "ft_sched.cpp"

// struct DeadlineWatcher {
// 	Slice<Deadline> deadlines;
// 	DeadlineHandle first;
//
// 	void clear(){
// 		for(u32 i = 0; i < deadlines.len; i += 1){
// 			auto s = deadlines[i]._slot;
//
// 			deadlines[i]._slot.idx = min<u32>(i + 1, DeadlineHandle::max_idx);
// 			deadlines[i]._slot.gen += min<u32>(s.gen + 1, DeadlineHandle::max_idx);
// 		}
// 		deadlines[deadlines.len-1]._slot = {0, 0};
// 	}
//
// 	bool add(RawTask* t, Duration limit){
// 		if(first.empty()){
// 			return false;
// 		}
//
// 		Deadline* d = &deadlines[first.idx];
// 		first = d->_slot;
//
// 		d->last_tick = tick_now();
// 		d->task = t;
// 		d->limit = limit;
//
// 		return DeadlineHandle{};
// 	}
//
// 	bool reset(DeadlineHandle h){
// 		if(h.idx < deadlines.len){
// 			return false;
// 		}
// 		auto d = &deadlines[h.idx];
// 		ensure(h.gen == d._slot.gen, "Dangling node");
// 		return d;
// 	}
//
// 	// void check(){}
// };
