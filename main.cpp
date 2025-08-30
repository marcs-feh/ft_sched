#include "base.hpp"

#include "ft_sched.hpp"
#include "crc32.gen.cpp"

extern "C" {
	int printf(char const*, ...);
}

// - Task interface

template<class T>
void print_list(List<T> const& list, char const* elem_fmt){
	printf("len: %td cap: %td [ ", list.len, list.cap);
	for(usize i = 0; i < list.len; i ++){
		printf(elem_fmt, list[i]);
		printf(" ");
	} printf("]\n");
}

using TaskFunc = void (*)(void*);

enum TaskStatus : u16 {
	Undefined = 0,
	Initialized = 1,
	Started = 2,
	Done = 3,

	Fault, // Or anthing above
};

// struct RawTask {
// 	u32 id;
// 	void* arg;
// 	TaskFunc func;
// };

template<typename Fn>
struct Task {
	Arena* arena;
	Fn body;

	void run(){
		body();
	}

	Task() = delete;
	Task(Fn body) : body{body}{}
};

template<typename Fn> [[nodiscard]]
Task<Fn>* make_task(Arena* a, Fn body){
	auto t = make<Task<Fn>>(a, body);
	if(!t){ panic("Failed to create task"); }
	t->arena = a;
	return t;
}

int main(){
	usize arena_size = 4 * mem_kilobyte;
	u8* arena_data = new u8[arena_size];

	Arena arena = arena_from_buffer({arena_data, arena_size});
	
	auto q = make_sync_queue<f32>(&arena, 64);

	auto t = make_task(&arena, [](){
		printf("Hello");
	});

	t->run();
}
