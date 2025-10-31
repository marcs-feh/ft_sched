#include <stdio.h>

#include "base.hpp"

#include "ft_sched.hpp"

extern "C" int puts(char const*);

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
	for(int i = 0;;i++){
		printf("Hi %d\r\n", i);
	}
}

template<typename Func, typename T>
concept ConsensusFunc = requires(Func cons, T const& x) {
	{ cons(x, x, x) } -> SameAs<bool>;
};

template<typename T, ConsensusFunc<T> Func>
Option<T> consensus(T&& a, T&& b, T&& c, Func&& f){
	auto ab = f(a, b);
	auto ac = f(a, c);
	auto bc = f(b, c);

	if(ab)
		return {forward<T>(a)};
	if(ac)
		return {forward<T>(a)};
	if(bc)
		return {forward<T>(b)};

	// No consensus, uh oh!
	return {};
}

// IMPORTANT: The tasks will be concurrently executed, it the caller's
// responsibility to ensure that whatever arguments were read, either by
// capture or explicitly are thread safe and do not interfere.
template<typename Output, Callable<Output> TaskFunc, Callable<void> OnCancel>
struct TMR_Task {
	struct SubTask {
		RawTask task;
		TaskFunc func;
		Option<Output> result;
		DeadlineSlot* slot = nullptr;
	};

	Array<SubTask, 3> _children;
	DeadlineWatcher* watcher;

	Duration deadline;

	struct Context {
		TMR_Task* self;
		DeadlineSlot* slot;
	};

	static void _task_wrapper(void* arg){
		auto sub_task = (SubTask*)arg;
		if(sub_task->slot){
			sub_task->slot.reset();
		}

		sub_task->result = sub_task->func();

		if(sub_task->slot){
			sub_task->slot.reset();
		}
	}

	void run(){
		for(int i = 0; i < 3; i ++){
			if(watcher){
				_children[i].slot = watcher->add(&_children[i], deadline);
			}
		}
	}

	void join(){
		unimplemented();
	}

	Option<Output> result(){
		unimplemented();
	}

	TMR_Task(TaskFunc&& f)
		: _children{{.func = f}, {.func = f}, {.func = f}}
		, watcher{nullptr}
		, deadline{0}
	{}
};


Arena main_arena;
constexpr usize main_arena_size = 4096;
u8 main_arena_memory[main_arena_size];

attribute_force_inline static inline
void entrypoint(){
	main_arena = arena_from_buffer({&main_arena_memory[0], main_arena_size});

	auto t = make_basic_task(&main_arena, [](){
		printf("Begin\n");
		sleep_for(Duration::from_milli(500));
		printf("End\n");
		return Unit{};
	});

	t->run();
	sleep_for(Duration::from_milli(900));
	t->cancel();
}

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

