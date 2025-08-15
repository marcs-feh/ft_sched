#include "base.hpp"

#include <stdio.h>

Slice<u8> file_read(String path, Arena* arena);
i64 file_write(String path, Slice<u8> data);

constexpr usize SCRATCH_ARENA_SIZE = 8 * 1024LL * 1024LL;

thread_local u8 scratch_arena_mem[SCRATCH_ARENA_SIZE];
thread_local Arena scratch_arena = {};

void init(){
	scratch_arena = arena_from_buffer(Slice<u8>{scratch_arena_mem, sizeof(scratch_arena_mem)});
}

int main(){
	init();
	constexpr u32 CRC32_POLYNOMIAL = 0x0;
	auto file_data = file_read("generate.cpp", &scratch_arena);
	printf("%.*s\n", str_fmt(String(file_data)));
	file_write("skibidi.txt", file_data);
}

i64 file_write(String path, Slice<u8> buf){
	FILE* fd = nullptr;

	/* Open file */ {
		auto reg = arena_region_begin(&scratch_arena);
		auto path_null = clone_to_cstring(path, &scratch_arena);
		fd = fopen(path_null, "wb");
		arena_region_end(reg);

		if(fd == nullptr){ return -1; }
	}

	return i64(fwrite(buf.data, 1, buf.len, fd));
}

Slice<u8> file_read(String path, Arena* arena){
	FILE* fd = nullptr;
	usize file_size = 0;
	Slice<u8> buf = {};

	/* Open file */ {
		auto reg = arena_region_begin(&scratch_arena);
		auto path_null = clone_to_cstring(path, &scratch_arena);
		fd = fopen(path_null, "rb");
		arena_region_end(reg);

		if(fd == nullptr){ return {}; }
	}

	fseek(fd, 0, SEEK_END);
	usize end = ftell(fd);
	rewind(fd);
	usize begin = ftell(fd);
	file_size = end - begin;

	buf = make_slice<u8>(arena, file_size + 1);
	if(buf.len != (file_size + 1)){
		goto exit;
	}
	buf.len -= 1;

	fread(buf.data, 1, file_size, fd);

exit:
	fclose(fd);
	return buf;
}
