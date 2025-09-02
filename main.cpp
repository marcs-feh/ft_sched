#include "base.hpp"

#include "ft_sched.hpp"
#include "crc32.gen.cpp"
#include <stdio.h>

using Tick = i64;

using Duration = i64;

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

enum TaskStatus : u16 {
	Undefined = 0,
	Initialized = 1,
	Started = 2,
	Done = 3,

	Fault, // Or anthing above
};

struct Executable {
	virtual void run() = 0;
};

Tick tick_current();

template<typename Fn>
struct Task : Executable {
	TaskStatus status;
	Fn body;
	// Deadline* deadline;

	void run() override {
		body();
	}

	Task() = delete;
	explicit Task(Fn body) : body{body}{}
};

struct Deadline {
	Atomic<Tick> limit;
	u32 duration;

	void reset(){
		limit.store(tick_current(), memory_order_relaxed);
	}
};

constexpr static usize MAX_DEADLINES = 16;

struct Watcher {
	SmallList<Deadline, MAX_DEADLINES> deadlines;
};

template<typename F>
concept Action = requires(F f){
	{ f() } -> SameAs<void>;
};

template<Action Fn> [[nodiscard]]
Executable* make_task(Arena* a, Fn body){
	auto t = make<Task<Fn>>(a, body);
	if(!t){ panic("Failed to create task"); }
	return t;
}

Tick tick_current(){
	// struct timespec ts = {};
	// clock_gettime(CLOCK_MONOTONIC, &ts);
	// i64 now = (ts.tv_sec * 1000000000LL) + ts.tv_nsec;
	// return Tick(now);
	return 0;
}

int main(){
	usize arena_size = 4 * mem_kilobyte;
	u8* arena_data = new u8[arena_size];

	Arena arena = arena_from_buffer({arena_data, arena_size});

	SmallList<i32, 30> nums;
	nums.append(5);
	nums.append(5);
	print_slice(nums.slice(), "%d");

	delete[] arena_data;
}
