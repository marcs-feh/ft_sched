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
	constexpr usize arena_size = 8 * mem_kilobyte;
	static u8 arena_memory[arena_size] = {0};

	auto arena = arena_from_buffer({arena_memory, arena_size});
	auto nums = make_list<f32>(&arena);

	for(f32 i = 0; i < 30; i++){
		print_list(nums, "%.1f");
		insert(&nums, i, 0);
	}

	for(usize i = 0; i < nums.len / 2; i++){
		print_list(nums, "%.1f");
		remove(&nums, 1);
		remove(&nums, nums.len - 1);
	}

	f32 n = 0;
	while(pop(&nums, &n)){
		printf("<<< %.1f ", n);
		print_list(nums, "%.1f");
	}

}