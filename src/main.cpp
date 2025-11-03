#include <stdio.h>

#include "base.hpp"

#include "ft_sched.hpp"

// #include "tmr_experimental.hpp"

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

Arena main_arena;
constexpr usize main_arena_size = 4096;
u8 main_arena_memory[main_arena_size];

attribute_force_inline static inline
void entrypoint(){
	// swdg::init(Duration::from_milli(1'000));

	#if defined(FT_SCHED_PLATFORM_STM32F411CEU6)
	for(int i = 5; i > 0; i --){
		sleep_for(Duration::from_milli(1'000)); printf("%d\r\n", i); fflush(stdout);
	}
	#endif

	main_arena = arena_from_buffer({&main_arena_memory[0], main_arena_size});

	DeadlineWatcher* watcher = make_deadline_watcher(&main_arena, 32);

	bool running = true;
//	auto watcher_task = make_basic_task(&main_arena, [watcher, &running](TaskContext){
//		while(running){
//			auto ok = watcher->scan();
//			if(ok){
//				swdg::reset_watchdog();
//			}
//			// printf("CHECK: %s\n", ok ? "OK" : "FAIL");fflush(stdout);
//
//			sleep_for(Duration::from_milli(1));
//		}
//
//		return Unit{};
//	});
//	watcher_task->run();

	// auto tmr0 = make_tmr_task(&main_arena, Duration::from_milli(33), [](TaskContext ctx){
	// 	if(ctx.id() == 6)
	// 		ctx.task->cancel();

	// 	printf("[TMR1] Hello, it's %d\n", ctx.task->id); fflush(stdout);
	// 	return 69;
	// });
	// printf("SPAWNED: %d\n", tmr0->supervisor.id);

	auto t = make_basic_task(&main_arena, [](TaskContext ctx){
		auto inner = make_basic_task(&main_arena, [](TaskContext ctx){
			printf("[INNER] Hello %d\r\n", int(ctx.id()));
			return Unit{};
		});

		inner->run();
		inner->join();

		printf("[OUTER] Hello %d\r\n", int(ctx.id()));
		fflush(stdout);

		return Unit{};
	});

	t->run();
	t->join();
	fflush(stdout);

	// tmr0->run();
	// tmr0->join();
	// printf("RESULT: %d\n", tmr0->result().unwrap());

	// char anim[] = {'-', '\\', '|', '/'};
	// u8 anim_frame = 0;
	printf("--- ENTRYPOINT END ---\r\n");

	while(1){
		#if !defined(FT_SCHED_NO_MAIN)
		break;
		#endif
	}
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

