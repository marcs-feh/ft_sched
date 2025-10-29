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
	DeadlineSlot* _next;
	DeadlineSlot* _prev;

	void reset(){
		last_tick = tick_now();
	}
};

struct DeadlineWatcher {
	Slice<DeadlineSlot> slots;
	DeadlineSlot* check_list_head;
	DeadlineSlot* free_list_head;
	Spinlock _lock{};

	[[nodiscard]]
	DeadlineSlot* add(RawTask* task, Duration limit){
		auto guard = _lock.guard();
		if(!free_list_head){
			return 0;
		}

		ensure(free_list_head->_prev == nullptr, "Invalid list state");

		auto node = free_list_head;

		// Pop head
		free_list_head = node->_next;
		if(free_list_head){
			free_list_head->_prev = nullptr;
		}

		// Push head
		node->_next = check_list_head;
		if(check_list_head){
			check_list_head->_prev = node;
		}
		check_list_head = node;

		node->limit = limit;
		node->last_tick = tick_now();
		node->task = task;

		return node;
	}

	void remove(DeadlineSlot* node){
		ensure(node >= slots.data && node < (slots.data + slots.len), "invalid slot");

		auto guard = _lock.guard();

		// Re-link siblings, if any
		auto next = node->_next;
		auto prev = node->_prev;

		if(next){
			next->_prev = node->_prev;
		}
		if(prev){
			prev->_next = node->_next;
		}

		// Re-insert into free list
		node->_next = free_list_head;
		if(free_list_head){
			free_list_head->_prev = node;
		}
		node->_prev = nullptr;
	}

	void clear(){
		auto guard = _lock.guard();

		mem_zero(slots.data, slots.len * sizeof(*slots.data));

		auto n = isize(slots.len);

		for(isize i = 0; i < n; i += 1){
			DeadlineSlot* prev = nullptr;
			DeadlineSlot* next = nullptr;

			if(i > 0){
				prev = &slots.data[i - 1];
			}
			if(i < n - 1){
				next = &slots.data[i + 1];
			}

			slots[i]._prev = prev;
			slots[i]._next = next;
		}
		check_list_head = nullptr;
		free_list_head = slots.data;
	}

	void scan(){
		auto guard = _lock.guard();

		auto now = tick_now();
		for(auto cur = check_list_head; cur != nullptr; cur = cur->_next){
			if(cur->task->status() == TaskStatus_Done){
				remove(cur);
				continue;
			}
			else {
				if(tick_diff(now, cur->last_tick) > cur->limit){
					u8 buf[80];
					auto msg =buffer_printf(Slice<u8>{&buf[0], sizeof(buf)}, "expired deadline on task %p", cur->task);
					panic(msg.data);
				}
			}
		}
	}

	DeadlineWatcher()
		: slots{}
		, check_list_head{nullptr}
		, free_list_head{nullptr}
		, _lock{}
		{}
};

void init_deadline_watcher(DeadlineWatcher* w, Slice<DeadlineSlot> slots){
	w->slots = slots;
	w->clear();
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
