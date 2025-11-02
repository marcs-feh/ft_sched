#include <stdio.h>

#include "base.hpp"

#include "ft_sched.hpp"

extern "C" int puts(char const*);

namespace swdg {
static Atomic<TimeTick> last_tick = 0;

static RawTask task;

static Duration limit = {0};

void watchdog_timer_func(RawTask*){
	last_tick = tick_now();

	while(1){
		auto now = tick_now();
		auto elapsed = tick_diff(now, last_tick.load(memory_order_relaxed));

		if(elapsed > limit){
			fprintf(stderr, "[swdg] Failed! limit=%tdms elapsed=%tdms\n", limit.to_milli(), elapsed.to_milli());
			trap();
		}
		sleep_for(Duration::from_milli(1));
	}
}

void reset_watchdog(){
	// printf("Reset WDG\n"); fflush(stdout);
	last_tick.store(tick_now(), memory_order_relaxed);
}

void init(Duration n){
	limit = n;
	init_raw_task(&task, nullptr, watchdog_timer_func, nullptr);
	task.run();
	printf("[swdg] Initialized\n");
}
}

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

void print_info(){
	u8 bufdata[48];
	auto buf = Slice<u8>(&bufdata[0], sizeof(bufdata));
	String msg;

	msg = buffer_printf(buf, "Address Width:  %zu-bit", sizeof(void*) * 8); puts(msg.data);
	msg = buffer_printf(buf, "Tick Frequency: %tu Hz", tick_frequency()); puts(msg.data);
	msg = buffer_printf(buf, "RawTask size:   %td", sizeof(RawTask)); puts(msg.data);
}

template<typename T>
using ConsensusFunc = bool (*)(T const&, T const&);

template<typename T>
int consensus(T& a, T& b, T& c, ConsensusFunc<T> f){
	auto ab = f(a, b);
	auto bc = f(b, c);
	auto ca = f(a, c);

	if(ab) return 0;
	if(bc) return 1;
	if(ca) return 2;

	// No consensus, uh oh!
	return -1;
}

template<typename T>
constexpr auto default_consensus_func = [](T const& a, T const& b) -> bool {
	return a == b;
};


// IMPORTANT: The tasks will be concurrently executed, it the caller's
// responsibility to ensure that whatever arguments were read, either by
// capture or explicitly are thread safe and do not interfere.
template<typename Output, Callable<Output, TaskContext> TaskFunc, Callable<void, TaskContext> OnCancel>
struct TMR_Task {
	struct SubTask {
		RawTask task;
		TaskFunc func;
		Option<Output> result;
	};

	RawTask supervisor;
	DeadlineWatcher watcher;
	Array<SubTask, 3> workers;
	Duration worker_deadline;
	ConsensusFunc<Option<Output>> consensus_func;

	static void _worker_wrapper(RawTask* t){
		auto sub_task = (SubTask*)t->args;
		auto ctx = TaskContext { &sub_task->task };
		sub_task->result = sub_task->func(ctx);
	}

	static void _supervisor_wrapper(RawTask* t){
		auto self = (TMR_Task<Output, TaskFunc, OnCancel>*)t->args;

		for(int i = 0; i < 3; i ++){
			self->workers[i].task.run();
		}

		while(self->watcher.count()){
			self->watcher.scan();
			sleep_for(Duration::from_milli(1));
		}
	}

	void run(){
		supervisor.run();
	}

	void join(){
		supervisor.join();
	}

	Option<Output> result(){
		if(supervisor.status() < TaskStatus_Done){
			return {};
		}

		int n = consensus(workers[0].result, workers[1].result, workers[2].result, consensus_func);
		if(n < 0){
			return {};
		}

		return move(workers[n].result);
	}

	void cancel(){
		for(int i = 0; i < 3; i++){
			workers.task[i].cancel();
		}
		supervisor.cancel();
	}

	TaskStatus status(){
		return supervisor._status.load(memory_order_relaxed);
	}

	RawTask* raw_task(){
		return &supervisor;
	}

	u32 id(){
		return supervisor.id;
	}

	TMR_Task()
		: supervisor{}
		, watcher{}
		, workers{}
		, worker_deadline{0}
		, consensus_func{default_consensus_func<Option<Output>>}
	{}
};

static_assert(Task<TMR_Task<Unit, decltype(_task_nop), decltype(_cancellation_nop)>, Unit>, "BasicTask does not conform to Task concept");

template<typename F>
auto make_tmr_task(Arena* arena, Duration worker_deadline, F&& func){
	auto t = make<TMR_Task<decltype(func(TaskContext{})), F, decltype(_cancellation_nop)>>(arena);
	auto slots = make_slice<DeadlineSlot>(arena, 3);
	ensure(t, "Failed to create TMR task");

	init_deadline_watcher(&t->watcher, slots);
	t->worker_deadline = worker_deadline;
	init_raw_task(&t->supervisor, arena, t->_supervisor_wrapper, t);

	for(int i = 0; i < 3; i++){
		t->workers[i].func = func;
		init_raw_task(&t->workers[i].task, arena, t->_worker_wrapper, &t->workers[i]);

		auto res = t->watcher.watch(&t->workers[i].task, t->worker_deadline);
		ensure(res, "Failed to watch");
	}

	return t;
}

Arena main_arena;
constexpr usize main_arena_size = 4096;
u8 main_arena_memory[main_arena_size];

Unit hello(TaskContext){
	return {};
}

attribute_force_inline static inline
void entrypoint(){
	swdg::init(Duration::from_milli(1'000));
	main_arena = arena_from_buffer({&main_arena_memory[0], main_arena_size});

	DeadlineWatcher* watcher = make_deadline_watcher(&main_arena, 32);

	bool running = true;
	auto watcher_task = make_basic_task(&main_arena, [watcher, &running](TaskContext){
		while(running){
			auto ok = watcher->scan();
			if(ok){
				swdg::reset_watchdog();
			}
			// printf("CHECK: %s\n", ok ? "OK" : "FAIL");fflush(stdout);

			sleep_for(Duration::from_milli(1));
		}

		return Unit{};
	});
	watcher_task->run();

	auto tmr0 = make_tmr_task(&main_arena, Duration::from_milli(33), [](TaskContext ctx){
		if(ctx.id() == 6)
			ctx.task->cancel();

		printf("[TMR1] Hello, it's %d\n", ctx.task->id); fflush(stdout);
		return 69;
	});
	watcher->watch(&tmr0->supervisor, Duration::from_milli(2000));
	printf("SPAWNED: %d\n", tmr0->supervisor.id);


	printf("START TASKS\n");
	tmr0->run();
	tmr0->join();

	printf("RESULT: %d\n", tmr0->result().unwrap());
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

