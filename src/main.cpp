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

	auto t = make_basic_task(&main_arena, [](){
		print_info();
		return i32(69);
	});

	t->run();
	t->join();
}

template<typename T>
concept CRC32_Checkable = requires(T const& obj) {
	{ crc32(obj) } -> SameAs<u32>
};

template<CRC32_Checkable T>
struct CRC32_Box {
	T _data;
	volatile u32 _crc;

	bool check() const {
		return crc32(_data) == _crc;
	}

	void recompute(){
		_crc = crc32(_data);
	}
};

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
