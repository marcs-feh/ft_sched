#include "base.hpp"

#include "ft_sched.hpp"
#include "task.hpp"
#include <stdio.h>

#include "crc32.gen.cpp"

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


template<TaskBody Fn> [[nodiscard]]
Executable* make_task(Arena* a, Fn body){
	auto t = make<Task<Fn>>(a, body);
	return t;
}

// struct Deadline {
// 	Atomic<Tick> limit;
// 	u32 duration;

// 	void reset(){
// 		limit.store(tick_current(), memory_order_relaxed);
// 	}
// };

// constexpr static usize MAX_DEADLINES = 16;

// template<class T>
// struct Pool {
// 	// IMPORTANT: Data MUST be the first member.
// 	struct Node { T data; Node* next; };

// 	Slice<Node> data = {};
// 	Node* free_list_head = nullptr;

// 	void clear(){
// 		this->free_list_head = nullptr;
// 		mem_zero(data.data, data.len * sizeof(data[0]));

// 		for(usize i = 0; i < this->data.len; i++){
// 			data[i].next = this->free_list_head;
// 			this->free_list_head = data[i];
// 		}
// 	}

// 	void destroy(T* elem){
// 		if(!elem){ return; }
// 		ensure((elem >= this->data.data) && (elem < this->data.data + this->data.len), "Element does not belong to collection");
// 		elem->~T();
// 		auto node = (Node*)elem; // IMPORTANT: Only possible because the layout of Node has data at the start!
// 		mem_zero(&node->data, sizeof(node->data));
// 		node->next = free_list_head;
// 		free_list_head = node;
// 	}

// 	template<typename ...Args>
// 	T* create(Args&& ... args){
// 		if(!free_list_head){
// 			return nullptr;
// 		}
// 		auto node = free_list_head;
// 		node->next = nullptr;
// 		free_list_head = node->next;
// 		new (&node->data) T(forward<Args>(args)...);
// 		return (T*)node;
// 	}
// };

// Tick tick_current(){
// 	return 0;
// }

int main(){
	usize arena_size = 4 * mem_kilobyte;
	u8* arena_data = new u8[arena_size];

	Arena arena = arena_from_buffer({arena_data, arena_size});

	auto runnables = make_list<Executable*>(&arena);
	int x = 0;
	
	// runnables.append(make_task(&arena, [](Executable*){
	// 	printf("Hello A\n");
	// }));
	// runnables.append(make_task(&arena, [&x](Executable*){
	// 	x += 5;
	// 	printf("Hello B %d\n", x);
	// }));
	// runnables.append(make_task(&arena, [](Executable* t){
	// 	printf("Hello C\n");
	// }));

	for(usize i = 0; i < runnables.len; i += 1){
		runnables[i]->run();
	}

	fflush(stdout);

	for(usize i = 0; i < runnables.len; i += 1){
		if(runnables[i]->status() == TaskStatus_Fault){
			panic("FAULTED");
		}
	}

	delete[] arena_data;
}
