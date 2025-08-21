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
	String s = "Hello";
}
