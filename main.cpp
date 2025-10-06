#include <stdio.h>

#include "base.hpp"

#include "ft_sched.hpp"
#include "task.hpp"

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

// template<TaskBody Fn> [[nodiscard]]
// Executable* make_task(Arena* a, Fn body){
// 	auto t = make<Task<Fn>>(a, body);
// 	return t;
// }

void somebody(RawTask* t){
	printf("Hello from task %p\n", t);
}

constexpr usize destructive_interference_size = 64;

template<typename T>
struct SPSC_Queue {
	T* data;
	usize capacity;

	alignas(destructive_interference_size)
	Atomic<usize> read_pos{0};
	alignas(destructive_interference_size)
	Atomic<usize> write_pos{0};

	bool try_push(T elem){
		auto cur_write_pos = this->write_pos.load(memory_order_relaxed);
		auto next_write_pos = (cur_write_pos + 1) & (capacity - 1); // NOTE: Fast modulo for powers of 2

		if(next_write_pos != this->read_pos.load(memory_order_acquire)){
			this->data[write_pos] = elem;
			this->write_pos.store(next_write_pos, memory_order_release);
			return true;
		}

		return false;
	}

	bool try_pop(T* out){
		auto cur_read_pos = this->read_pos.load(memory_order_relaxed);
		if(cur_read_pos == this->write_pos.load(memory_order_acquire)){
			return false;
		}

		if(out != nullptr){
			*out = move(this->data[cur_read_pos]);
		}

		auto next_read_pos = (cur_read_pos + 1) & (capacity - 1);
		this->read_pos.store(next_read_pos, memory_order_release);
		return true;
	}
};

// init_spsc_queue()

int main(){
	usize arena_size = 4 * mem_kilobyte;
	u8* arena_data = new u8[arena_size];

	Arena arena = arena_from_buffer({arena_data, arena_size});
	printf("Sizeof RawTask: %tu\n", sizeof(RawTask));

	// auto runnables = make_list<Task*>(&arena);

	// for(usize i = 0; i < 20; i++){
	// 	auto task = make_raw_task(&arena, somebody, nullptr, 0);
	// 	runnables.append(task);
	// }

	// for(usize i = 0; i < runnables.len; i++){
	// 	runnables[i]->run();
	// }

	// for(usize i = 0; i < runnables.len; i++){
	// 	runnables[i]->join();
	// }

}

