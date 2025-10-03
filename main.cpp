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
	printf("Hello from task\n");
}

int main(){
	usize arena_size = 4 * mem_kilobyte;
	u8* arena_data = new u8[arena_size];

	Arena arena = arena_from_buffer({arena_data, arena_size});
	defer(printf("deferred hello!\n"));

	auto runnables = make_list<Task*>(&arena);

	for(usize i = 0; i < 20; i++){
		auto task = make_raw_task(&arena, somebody, nullptr, 0);
		printf("Add: %p\n", task);
		runnables.append(task);
	}

	for(usize i = 0; i < runnables.len; i++){
		runnables[i]->run();
	}

	for(usize i = 0; i < runnables.len; i++){
		runnables[i]->join();
	}

}

