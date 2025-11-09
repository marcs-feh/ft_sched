#include "base.hpp"
#include "ft_sched.hpp"

static Atomic<TimeTick> swdg_last_tick = 0;

static RawTask swdg_task;

static Duration swdg_limit = {0};

static Atomic<bool> swdg_active = false;

static inline
void swdg_timer_func(RawTask*){
	swdg_last_tick = tick_now();

	while(1){
		if(swdg_active.load(memory_order_relaxed)){
			auto now = tick_now();
			auto elapsed = tick_diff(now, swdg_last_tick.load(memory_order_relaxed));

			if(elapsed > swdg_limit){
				fprintf(stderr, "[swdg] failed! limit=%tdms elapsed=%tdms\n", swdg_limit.to_milli(), elapsed.to_milli());
				trap();
			}
		}
		sleep_for(Duration::from_milli(1));
	}
}

void swdg_init(Duration n){
	constexpr usize arena_size = 100 * sizeof(void*);

	static Arena arena = {};
	static bool initialized = false;
	static u8 arena_memory[arena_size];

	if(!initialized){
		arena = arena_from_buffer({&arena_memory[0], arena_size});
		swdg_limit = n;
		auto ok = init_raw_task(&swdg_task, &arena, arena_size - sizeof(Arena) - 8, 0, swdg_timer_func, nullptr);
		ensure(ok, "Failed to init software watchdog");
		initialized = true;
		printf("[swdg] Initialized\n");
	}
}


void swdg_reset(){
	swdg_last_tick.store(tick_now(), memory_order_relaxed);
}

void swdg_stop(){
	swdg_active.store(false, memory_order_relaxed);
}

void swdg_resume(){
	swdg_last_tick.store(tick_now(), memory_order_relaxed);
	swdg_active.store(true, memory_order_relaxed);
}
