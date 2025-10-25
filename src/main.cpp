#if 0
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

// static
// i32 randint(i32 a, i32 b){
//     return (rand() % (b - a + 1)) + a;
// }

// init_spsc_queue()
struct Unit{};

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
	printf("RawTask size:   %td\n", sizeof(RawTask));
}

TimeTick start = 0;
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

template<typename F, typename Output>
concept Returns = requires(F f){
	{ f() } -> SameAs<Output>;
};

template<typename Output, Returns<Output> TaskFunc>
struct SimpleTask {
	RawTask _task;
	TaskFunc _func;
	Option<Output> _result;

	static void _simple_task_wrapper(RawTask* t){
		auto self = (SimpleTask<Output, TaskFunc>*)t->args;
		self->_result = Output{ self->_func() };
	}

	void run(){
		ensure(_task._status.load(memory_order_relaxed) == TaskStatus_Initialized, "Inner task has not been initialized");
		_task.run();
	}

	Output result(){
		if(_task._status.load() != TaskStatus_Done){
			_task.join();
		}
		return _result.unwrap();
	}

	bool has_result() const {
		return _result.ok() && _task._status.load(memory_order_relaxed) == TaskStatus_Done;
	}

	attribute_force_inline
	TaskStatus status() const {
		return _task._status.load(memory_order_relaxed);
	}

	explicit SimpleTask(TaskFunc f)
		: _task{}
		, _func{f} {}
};

template<typename F>
auto make_simple_task(Arena* a, F&& func){
	auto t = make<SimpleTask<decltype(func()), F>>(a, forward<F>(func));

	init_raw_task(&t->_task, a, t->_simple_task_wrapper, t);
	return t;
}

struct DeadlineHandle {
	u32 idx : 20;
	u32 gen : 12;

	static constexpr u32 max_idx = (1 << 20) - 1;
	static constexpr u32 max_gen = (1 << 12) - 1;

	constexpr bool empty() const {
		return gen == 0;
	}
};

static_assert(sizeof(DeadlineHandle) == 4, "Unexpected handle size");

struct Deadline {
	TimeTick last_tick{0};
	Duration limit{0};
	RawTask* task{nullptr};
};

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

int main(){
	srand(tick_now());
	permanent_arena = arena_from_buffer(Slice<u8>{&permanent_arena_data[0], sizeof(permanent_arena_data)});
	start = tick_now();
	usize arena_size = 8 * mem_kilobyte;
	u8* arena_data = new u8[arena_size];

	Arena arena = arena_from_buffer(Slice<u8>{&arena_data[0], arena_size});
	print_info();

	auto foo = make_simple_task(&arena, [](){return 1;});
	foo->run();
	int n = foo->result();
	printf("Foo: %d\n", n);
	printf("Sizeof handle: %td\n", sizeof(DeadlineHandle));
	printf("Sizeof foo: %td (overhead: %td)\n", sizeof(*foo), sizeof(*foo) - sizeof(RawTask));
}
#endif

#include "base.hpp"
extern "C" {
	int printf(char const*, ...);
}

template<class T>
void print_slice(Slice<T> slice, char const* elem_fmt){
	printf("len: %td [ ", slice.len);
	for(usize i = 0; i < slice.len; i ++){
		printf(elem_fmt, slice[i]);
		printf(" ");
	} printf("]\r\n");
}

Arena main_arena;
constexpr usize main_arena_size = 4096;
u8 main_arena_memory[main_arena_size];

void entrypoint(){
	main_arena = arena_from_buffer({&main_arena_memory[0], main_arena_size});
	auto s = make_slice<i32>(&main_arena, 69);
	for(usize i = 0; i < s.len; i+=1){
		s[i] = i;
	}

	print_slice(s, "%d");
}

extern "C" {
void ft_sched_entrypoint(){
	entrypoint();
}
}

#if 0
int main() {
	ft_sched_entrypoint();
}
#endif