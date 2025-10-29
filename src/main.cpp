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

struct DeadlineSlot {
	int value;

	TimeTick last_tick;
	Duration limit;
	RawTask* task;

	void reset(){
		last_tick = tick_now();
	}
};

struct DeadlineWatcher {
	Slice<DeadlineSlot> slots;
	Spinlock _lock{};


	auto lock_guard(){
		return _lock.guard();
	}

	[[nodiscard]]
	DeadlineSlot* add(RawTask* task, Duration limit){
		auto guard = _lock.guard();

		DeadlineSlot* free_slot = nullptr;

		for(auto& slot : slots){
			if(slot.task == nullptr){
				free_slot = &slot;
			}
		}

		if(free_slot){
			free_slot->task = task;
			free_slot->limit = limit;
		}

		return free_slot;
	}

	void _remove_no_lock(DeadlineSlot* node){
		if(!node){ return; }
		ensure(node >= slots.data && node < (slots.data + slots.len), "invalid slot");

		node->task = nullptr;
		node->limit = {0};
	}

	void remove(DeadlineSlot* node){
		auto g = _lock.guard();
		_remove_no_lock(node);
	}

	void clear(){
		auto guard = _lock.guard();
		mem_zero(slots.data, slots.len * sizeof(*slots.data));
	}

	// Scan for deadline violations and remove Done tasks
	void scan(){
		auto guard = _lock.guard();

		auto now = tick_now();
		for(auto& slot : slots){
			if(slot.task->status() == TaskStatus_Done){
				_remove_no_lock(&slot);
				continue;
			}

			if(tick_diff(now, slot.last_tick) > slot.limit){
				u8 buf[80];
				auto msg =buffer_printf(Slice<u8>{&buf[0], sizeof(buf)}, "expired deadline on task %p", slot.task);
				panic(msg.data);
			}
		}
	}

	DeadlineWatcher()
		: slots{}
		, _lock{}
	{}
	

	void display(){
		printf("Free: ");
		for(auto const& slot : slots){
			if(!slot.task){
				printf(" . ");
			}
		}
		printf("\n");

		printf("Check: ");
		for(auto const& slot : slots){
			if(slot.task){
				printf("%d ", (int)slot.limit.to_milli());
			}
		}
		printf("\n");
	}
};

void init_deadline_watcher(DeadlineWatcher* w, Slice<DeadlineSlot> slots){
	mem_zero(w, sizeof(DeadlineWatcher));
	w->slots = slots;
	w->clear();
}

DeadlineWatcher* make_deadline_watcher(Arena* a, usize slot_count){
	auto restore = a->offset;
	auto watcher = make<DeadlineWatcher>(a);
	auto slots = make_slice<DeadlineSlot>(a, slot_count);

	if(!watcher || !slots){
		a->offset = restore;
		return nullptr;
	}

	watcher->slots = slots;
	watcher->clear();
	return watcher;
}

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

	auto watcher = make_deadline_watcher(&main_arena, 10);
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
