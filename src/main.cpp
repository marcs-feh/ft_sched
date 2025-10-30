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

template<typename Output, Callable<Output> TaskFunc>
struct ReexecTask {
	RawTask _task;
	TaskFunc _func;
	Option<Output> results[3];
	Atomic<u8> execution_counter;

	static void _task_wrapper(RawTask* t){
		auto self = (ReexecTask<Output, TaskFunc>*)t->args;
		auto n = execution_counter.load() - 1;
		self->_results[n] = Output{ self->_func() };
	}

	void run(){
		execution_counter.fetch_add(1);
	}

	Option<T> result() {
		unimplemented();
	}

	TaskStatus status() const {
		return _task.status();
	}

	void cancel(){
		unimplemented();
	}

	void join(){
		_task.join();
	}

	ReexecTask(TaskFunc&& f)
		: _task{}
		, _func{forward<TaskFunc>(f)}
		, results{}
		, execution_counter{0}
	{}
};

template<typename F>
auto make_reexec_task(Arena* a, F&& func){
	auto t = make<ReexecTask<decltype(func()), F/*, decltype(_cancellation_nop)*/>>(a, forward<F>(func));
	init_raw_task(&t->_task, a, t->_task_wrapper, t);
	// t->_task.on_cancel = t->_task_cancel_wrapper;
	return t;
}


Arena main_arena;
constexpr usize main_arena_size = 4096;
u8 main_arena_memory[main_arena_size];

attribute_force_inline static inline
void entrypoint(){
	main_arena = arena_from_buffer({&main_arena_memory[0], main_arena_size});

	auto t = make_reexec_task(&main_arena, [](){
		printf("Hello\n");
		sleep_for(Duration::from_milli(500));
		return Unit{};
	});

	t->run();
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

