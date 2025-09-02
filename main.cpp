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

template<class T>
struct Pool {
	// IMPORTANT: Data MUST be the first member.
	struct Node { T data; Node* next; };

	Slice<Node> data = {};
	Node* free_list_head = nullptr;

	void clear(){
		this->free_list_head = nullptr;
		mem_zero(data.data, data.len * sizeof(data[0]));

		for(usize i = 0; i < this->data.len; i++){
			data[i].next = this->free_list_head;
			this->free_list_head = data[i];
		}
	}

	void destroy(T* elem){
		if(!elem){ return; }
		ensure((elem >= this->data.data) && (elem < this->data.data + this->data.len), "Element does not belong to collection");
		elem->~T();
		auto node = (Node*)elem; // IMPORTANT: Only possible because the layout of Node has data at the start!
		mem_zero(&node->data, sizeof(node->data));
		node->next = free_list_head;
		free_list_head = node;
	}

	template<typename ...Args>
	T* create(Args&& ... args){
		if(!free_list_head){
			return nullptr;
		}
		auto node = free_list_head;
		node->next = nullptr;
		free_list_head = node->next;
		new (&node->data) T(forward<Args>(elem)...);
		return (T*)node;
	}
};

// struct Watcher {
// 	Spinlock lock;
// 	SmallList<Deadline, MAX_DEADLINES> deadlines;

// 	Deadline* new_deadline(u32 duration){
// 		// auto now = tick_current();
// 		// Deadline d = {
// 		// 	.limit = now + duration,
// 		// 	.duration = duration,
// 		// };
// 		// this->deadlines.append();
// 	}
// };

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
