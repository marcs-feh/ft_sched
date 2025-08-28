#include "base.hpp"

#include "ft_sched.hpp"
#include "crc32.gen.cpp"

extern "C" {
	int printf(char const*, ...);
}

// - SPSC bounded atomic queue
// - Task interface

template<class T>
void print_list(List<T> const& list, char const* elem_fmt){
	printf("len: %td cap: %td [ ", list.len, list.cap);
	for(usize i = 0; i < list.len; i ++){
		printf(elem_fmt, list[i]);
		printf(" ");
	} printf("]\n");
}

int main(){
	usize arena_size = 4 * mem_kilobyte;
	u8* arena_data = new u8[arena_size];

	Arena arena = arena_from_buffer({arena_data, arena_size});
	
	auto q = make_sync_queue<f32>(&arena, 64);
}
