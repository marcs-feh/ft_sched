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

constexpr auto CRC_SOURCE = "";

struct CRC32_Table {
	u32 entries[256];
};

void crc32_fill_table(CRC32_Table* table, u32 polynomial) {
	constexpr u32 bit_width = sizeof(u32) * 8;
	constexpr u32 top_bit = (u32(1) << (bit_width - 1));
    u32 remainder = 0;

    for (int dividend = 0; dividend < 256; dividend += 1) {
        remainder = dividend << (bit_width - 8);

        for (u8 bit = 8; bit > 0; --bit){
            if (remainder & top_bit) {
                remainder = (remainder << 1) ^ polynomial;
            }
            else {
                remainder = (remainder << 1);
            }
        }
        table->entries[dividend] = remainder;
    }
}

struct StringBuilder {
	List<u8> buf;
};

StringBuilder builder_create(usize cap, Allocator alloc){
	StringBuilder sb = { make_list<u8>(alloc, 0, cap) };
	return sb;
}

bool builder_append(StringBuilder* sb, String str){
	if((sb->buf.len + str.len) >= sb->buf.cap){
		if(!resize(&sb->buf, max<usize>(16, sb->buf.len + str.len))){
			return false;
		}
	}

	mem_copy(&sb->buf.data[sb->buf.len], str.data, str.len);
	sb->buf.len += str.len;
	return true;
}

String builder_string(StringBuilder const& sb){
	return String((char const*)sb.buf.data, sb.buf.len);
}

Slice<u8> builder_bytes(StringBuilder const& sb){
	return slice(sb.buf);
}

int main(){
	init();
	auto allocator = arena_allocator(&scratch_arena);
	/* Generate CRC32 */ {
		constexpr u32 CRC32_POLYNOMIAL = 0x10101010;
		auto sb = builder_create(512, allocator);
		auto base_impl = String(file_read("assets/crc32.cpp", &scratch_arena));

		ensure(base_impl.data, "Failed to read crc32 file.");

		// str_printf("constexpr u32 CRC32_POLYNOMIAL = 0x%x;\n", CRC32_POLYNOMIAL);
		auto poly_decl = arena_printf(&scratch_arena, "constexpr u32 CRC32_POLYNOMIAL = 0x%08x;\n", CRC32_POLYNOMIAL);
		ensure(builder_append(&sb, poly_decl), "Shit.");
		ensure(builder_append(&sb, base_impl), "Shit.");

		i64 written = file_write("crc32.gen.cpp", builder_bytes(sb));
		ensure(written > 0, "Failed to write file");
	}
}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
