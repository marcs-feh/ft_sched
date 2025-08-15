#include "base.hpp"

extern "C" {
	int printf(char const*, ...);
}

// - CRC32
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
	auto nums = make_list<f32>(heap_allocator());

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