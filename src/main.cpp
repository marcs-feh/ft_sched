#include <stdio.h>

#include "base.hpp"

#include "ft_sched.hpp"

// #include "tmr_experimental.hpp"

struct SystemStats {
	Atomic<i32> failed_assertions = 0;
	Atomic<i32> crc_failures = 0;
	Atomic<i32> total_stack_space = 0;
	Atomic<i32> total_arena_space = 0;

	void dump(){
		constexpr usize stat_dump_size = 512;
		static u8 stat_dump_memory[stat_dump_size];

		auto res = buffer_printf({&stat_dump_memory[0], stat_dump_size},
			"failed_assertions: %d\r\n"
			"crc_failures: %d\r\n"
			"total_stack_space: %d\r\n"
			"total_arena_space: %d\r\n"
			, int(failed_assertions.load()), int(crc_failures.load()), int(total_stack_space.load()), int(total_arena_space.load()));

		fflush(stdout);
		printf("%s\r\n", res.data);
		fflush(stdout);
	}
};

SystemStats sys_statistics;

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

Arena task_arena{};
Arena main_arena{};

constexpr usize max_task_count = 10;
constexpr usize average_stack_size = 200 * sizeof(usize);
constexpr usize task_arena_size = (max_task_count * average_stack_size) + 1024;

u8 task_arena_memory[task_arena_size];

constexpr usize main_arena_size = 4096;
u8 main_arena_memory[main_arena_size];

void print_info(){
	static u8 bufdata[50];
	auto buf = Slice<u8>(&bufdata[0], sizeof(bufdata));
	String msg;

	msg = buffer_printf(buf, "[System Info]"); printf("%s\r\n", msg.data);
	msg = buffer_printf(buf, "  Task Arena Size: %zuB", task_arena_size); printf("%s\r\n", msg.data);
	msg = buffer_printf(buf, "  Address Width:   %zu-bit", sizeof(void*) * 8); printf("%s\r\n", msg.data);
	msg = buffer_printf(buf, "  Tick Frequency:  %tu Hz", tick_frequency()); printf("%s\r\n", msg.data);
	msg = buffer_printf(buf, "  RawTask size:    %td", sizeof(RawTask)); printf("%s\r\n", msg.data);
}

#include <math.h>

constexpr f64 pi = 3.14159265358979323846264338327950288;
constexpr f64 tau = 2.0 * pi;
constexpr f64 euler = 2.71828182845904523536028747135266249;

constexpr f64 gaussian(f64 peak, f64 stddev, f64 x){
	auto exponent = - (x*x) / (2 * stddev * stddev);
	return peak * exp(exponent);
}

attribute_force_inline static inline
void entrypoint(){
	main_arena = arena_from_buffer({&main_arena_memory[0], main_arena_size});
	task_arena = arena_from_buffer({&task_arena_memory[0], task_arena_size});

	constexpr f64 freq_mult = 2.0;
	constexpr usize samples = 21;
	auto signal = make_slice<f64>(&main_arena, samples);
	for(int t = 0; t < signal.len; t++){
		signal[t] = gaussian(1.0, 0.25, (f64(t) / samples) - 0.5);
	}

	print_slice(signal, "%0.02f");

	/*
	#if defined(FT_SCHED_PLATFORM_STM32F411CEU6)
	for(int i = 5; i > 0; i --){
		sleep_for(Duration::from_milli(1'000)); printf("%d\r\n", i); fflush(stdout);
	}
	#else
	swdg_init(Duration::from_milli(1'000));
	DeadlineWatcher* watcher = make_deadline_watcher(&task_arena, 32);
	ensure(watcher != nullptr, "Failed to create watcher");
	auto watcher_task = make_basic_task(&task_arena, [watcher](TaskContext){
		while(1){
			auto ok = watcher->scan();
			if(ok){
				swdg_reset();
			}

			sleep_for(Duration::from_milli(1));
		}

		return Unit{};
	});
	#endif

	print_info();
	auto queue = make_spsc_queue<i32>(&main_arena, 32);

	bool tasks_running = true;
	auto producer = make_basic_task(&task_arena, [&tasks_running, queue](TaskContext ctx){
		int n = 0;
		while(tasks_running){
			queue->try_push(n);
			n += 1;
			sleep_for(Duration::from_milli(100));
		}

		return Unit{};
	});

	auto consumer = make_basic_task(&task_arena, [&tasks_running, queue](TaskContext ctx){
		while(tasks_running){
			auto p = queue->pop();

			if(p){
				printf("%d\r\n", 2 * p.unwrap());
			}
			else {
				printf("-\r\n");
			}

			sleep_for(Duration::from_milli(100));
		}

		return Unit{};
	});

	printf("----- FINISHED -----\r\n");
	fflush(stdout);

	while(1){
		sleep_for({0});
	}
	*/
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

