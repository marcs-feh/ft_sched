#include "base.hpp"

#define STB_SPRINTF_IMPLEMENTATION
#include "vendor/stb_sprintf.h"

//// Basic types & utilities
usize cstring_len(cstring cs) {
	usize n = 0;
	while(cs[n] != 0){
		n += 1;
	}
	return n;
}

//// Assertions
extern "C"{
	[[noreturn]] void abort();
	#include <stdio.h>
}

void panic_ex(char const* msg, char const* filename, int line){
	fprintf(stderr, "(%s:%d) Panic: %s\n", filename, line, msg);
	abort();
}

bool ensure_ex(bool pred, char const* msg, char const* filename, int line){
	if(!pred){
		fprintf(stderr, "(%s:%d) Assertion failed: %s\n", filename, line, msg);
		abort();
	}
	return pred;
}

//// Memory
extern "C" {
	void* memmove(void* dest, void const* src, size_t n);
	void* memcpy(void* dest, void const* src, size_t n);
	void* memset(void* dest, int v, size_t n);
	int   memcmp(void const* lhs, void const* rhs, size_t n);
}

void* mem_copy(void* dest, void const* src, isize n){
	return memmove(dest, src, n);
}

void* mem_copy_no_overlap(void* dest, void const* src, isize n){
	return memcpy(dest, src, n);
}

void* mem_zero(void* dest, isize n){
	return memset(dest, 0, n);
}

void* mem_set(void* dest, u8 v, isize n){
	return memset(dest, v, n);
}

isize mem_compare(void const* lhs, void const* rhs, isize n){
	return memcmp(lhs, rhs, n);
}

uintptr mem_align_forward_ptr(uintptr p, uintptr a){
	ensure(mem_valid_alignment(a), "Alignment must be a power of 2 greater than 0");
	uintptr mod = p & (a - 1); /* Fast modulo for powers of 2 */
	if(mod > 0){
		p += (a - mod);
	}
	return p;
}

//// Arena
void arena_reset(Arena* a){
	ensure(a->region_count == 0, "Arena has dangling regions");
	a->offset = 0;
	a->last_allocation = nullptr;
}

bool arena_owns(Arena* a, void* p){
	uintptr ptr = (uintptr)p;
	uintptr lo = (uintptr)a->data;
	uintptr hi = lo + a->capacity;

	return (ptr >= lo) && (ptr <= hi);
}

void* arena_alloc(Arena* a, usize size, usize align){
	if(size == 0){ return nullptr; }
	uintptr base = (uintptr)a->data;
	uintptr current = base + (uintptr)a->offset;

	usize available = a->capacity - (current - base);

	uintptr aligned  = mem_align_forward_ptr(current, align);
	uintptr padding  = aligned - current;
	uintptr required = padding + size;

	if(required > available){
		return nullptr; /* Out of memory */
	}

	a->offset += required;
	void* allocation = (void*)aligned;
	a->last_allocation = allocation;
	mem_zero(allocation, size);

	return allocation;
}

void* arena_realloc(Arena* a, void* ptr, usize old_size, usize new_size, usize align){
	if(ptr == nullptr){
		return arena_alloc(a, new_size, align);
	}
	ensure(arena_owns(a, ptr), "Pointer not owned by arena");

	bool in_place = arena_resize(a, ptr, new_size);
	if(in_place){
		return ptr;
	}
	else {
		void* new_data = arena_alloc(a, new_size, align);
		if(new_data == nullptr){ return nullptr; } /* Out of memory */
		mem_copy(new_data, ptr, min(old_size, new_size));

		return new_data;
	}
}

bool arena_resize(Arena* a, void* ptr, usize new_size){
	if(ptr == nullptr){ return false; }
	ensure(arena_owns(a, ptr), "Pointer not owned by arena");

	uintptr base = (uintptr)a->data;

	if(ptr == a->last_allocation){
		uintptr last_alloc = (uintptr)a->last_allocation;

		if((last_alloc + new_size) > (base + a->capacity)){
			return false; /* No space left */
		}

		a->offset = (last_alloc + new_size) - base;
		// TODO: fill excess with 0s when increasing the allocation size
		return true;
	}

	return false;
}

Arena arena_from_buffer(Slice<u8> buf){
	Arena a;
	a.data = (void*)buf.data;
	a.offset = 0;
	a.capacity = buf.len;
	a.last_allocation = nullptr;
	a.region_count = 0;
	return a;
}

ArenaRegion arena_region_begin(Arena* a){
	ArenaRegion reg = {
		.arena = a,
		.offset = a->offset,
	};
	a->region_count += 1;
	return reg;
}

void arena_region_end(ArenaRegion reg){
	ensure(reg.arena->region_count > 0, "Arena has a improper region counter");
	ensure(reg.arena->offset >= reg.offset, "Arena has a lower offset than region");

	reg.arena->offset = reg.offset;
	reg.arena->region_count -= 1;
}

static
void* arena_allocator_func(void* data, AllocatorMode mode, void* ptr, usize old_size, usize new_size, usize align){
	auto arena = (Arena*)data;
	void* result = nullptr;

	switch(mode){
	case AllocatorMode_Alloc:
		result = arena_alloc(arena, new_size, align);
	break;

	case AllocatorMode_Realloc:
		result = arena_realloc(arena, ptr, old_size, new_size, align);
	break;

	case AllocatorMode_Free:
		arena_resize(arena, ptr, 0);
	break;

	case AllocatorMode_FreeAll:
		arena_reset(arena);
	break;

	default: panic("Invalid mode");
	}

	return result;
}

Allocator arena_allocator(Arena* a){
	return Allocator{arena_allocator_func, a};
}

//// String
String slice(String s) {
	return s;
}

String slice(String s, usize start, usize end) {
	ensure(end <= s.len && end >= start, "Invalid slicing indices");
	return String(&s.data[start], end - start);
}

String take(String s, usize count) {
	ensure(count <= s.len, "Cannot take more than string length");
	return String(s.data, count);
}

String skip(String s, usize count) {
	ensure(count <= s.len, "Cannot skip more than string length");
	return String(&s.data[count], s.len - count);
}

String clone(Arena* arena, String s){
	String res = {};
	u8* buf = (u8*)arena_alloc(arena, s.len, alignof(char));
	if(buf == nullptr){ return res; }
	mem_copy_no_overlap(buf, s.data, s.len);

	res.data = (char const*)buf;
	res.len = s.len;

	return res;
}

cstring clone_to_cstring(String s, Arena* a){
	auto buf = make_slice<u8>(a, s.len + 1);
	if(buf.data){
		mem_copy(buf.data, s.data, s.len);
		buf.data[s.len] = 0;
	}
	return (cstring)buf.data;
}

#define MASKX 0x3f /* 0011_1111 */
#define MASK2 0x1f /* 0001_1111 */
#define MASK3 0x0f /* 0000_1111 */
#define MASK4 0x07 /* 0000_0111 */

#define CONT_LO 0x80
#define CONT_HI 0xbf

struct UTF8AcceptRange { u8 lo, hi; };
 
static const
struct UTF8AcceptRange utf8_accept_ranges[5] = {
	{0x80, 0xbf},
	{0xa0, 0xbf},
	{0x80, 0x9f},
	{0x90, 0xbf},
	{0x80, 0x8f},
};

static const u8 utf8_accept_sizes[256] = {
	0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
	0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
	0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
	0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
	0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
	0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
	0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
	0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
	0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,
	0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,
	0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,
	0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,
	0xf1,0xf1,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x13,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x23,0x03,0x03,
	0x34,0x04,0x04,0x04,0x44,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,
};

RuneDecoded rune_decode(u8 const* buf, u32 buflen){
	RuneDecoded result = {};
	const RuneDecoded error = { .codepoint = RUNE_ERROR, .size = 1 };

	if(buflen < 1){
		return result;
	}

	u8 b0 = buf[0];
	u8 x = utf8_accept_sizes[b0];

	// ASCII or invalid
	if(x >= 0xf0){
		u32 mask = ((rune)(x) << 31) >> 31; // Either all 0's or all 1's to avoid branching
		result.codepoint = ((rune)(b0) & ~mask) | (RUNE_ERROR & mask);
		result.size = 1;
		return result;
	}

	u8 sz = x & 7;
	struct UTF8AcceptRange accept = utf8_accept_ranges[x >> 4];

	if(buflen < sz){
		return error;
	}

	u8 b1 = buf[1];
	if(b1 < accept.lo || accept.hi < b1){
		return error;
	}
	if(sz == 2){
		result.codepoint = ((rune)(b0 & MASK2) << 6) | ((rune)(b1 & MASKX));
		result.size = 2;
		return result;
	}

	u8 b2 = buf[2];
	if(b2 < CONT_LO || CONT_HI < b2){
		return error;
	}

	if(sz == 3){
		result.codepoint = ((rune)(b0 & MASK3) << 12) | ((rune)(b1 & MASKX) << 6) | (rune)(b2 & MASKX);
		result.size = 3;
		return result;
	}

	u8 b3 = buf[3];
	if(b3 < CONT_LO || CONT_HI < b3){
		return error;
	}

	result.codepoint = ((rune)(b0 & MASK4) << 18) | ((rune)(b1 & MASKX) << 12) | ((rune)(b2 & MASKX) << 6) | (rune)(b3 & MASKX);
	result.size = 4;
	return result;
}

RuneEncoded rune_encode(rune r){
	const u8 mask = 0x3f;
	RuneEncoded result = {};

	if(r <= 0x7f){ // 1-wide (ASCII)
		return { .bytes = {(u8)r}, .size = 1 };
	}

	if(r <= 0x7ff){ // 2-wide
		result.bytes[0] = 0xc0 |  (u8)(r >> 6);
		result.bytes[1] = 0x80 | ((u8)(r) & mask);
		result.size = 2;
		return result;
	}

	// Surrogate or invalid -> Encode the error rune
	if((r > 0x10ffff) || ((0xd800 <= r) && (r <= 0xdfff))){
		r = 0xfffd;
	}

	if(r <= 0xffff){ // 3-wide
		result.bytes[0] = 0xe0 |  (u8)(r >> 12);
		result.bytes[1] = 0x80 | ((u8)(r >> 6) & mask);
		result.bytes[2] = 0x80 | ((u8)(r)      & mask);
		result.size = 3;
		return result;
	}
	else { // 4-wide
		result.bytes[0] = 0xf0 |  (u8)(r >> 18);
		result.bytes[1] = 0x80 | ((u8)(r >> 12) & mask);
		result.bytes[2] = 0x80 | ((u8)(r >> 6)  & mask);
		result.bytes[3] = 0x80 | ((u8)(r)       & mask);
		result.size = 4;
		return result;
	}
}

String str_vprintf(Arena* arena, char const* fmt, va_list args){
	void* base = (void*)((uintptr)arena->data + arena->offset);
	usize size = arena->capacity - arena->offset;

	int n = stbsp_vsnprintf((char*)base, size - 1, fmt, args);
	if(n > 0){
		arena->offset += n + 1; /* Account for nullptr terminator */
		return String((char const*)base, n);
	}
	return {};
}

String str_printf(Arena* arena, char const* fmt, ...){
	va_list args;
	va_start(args, fmt);
	String res = str_vprintf(arena, fmt, args);
	va_end(args);
	return res;
}


extern "C"{
	void* malloc(size_t);
	void* realloc(void*, size_t);
	void free(void*);
}

//// Heap
static
void* heap_allocator_func(void*, AllocatorMode mode, void* ptr, usize old_size, usize new_size, usize align){
	void* result = nullptr;

	switch(mode){
	case AllocatorMode_Alloc:
		if(align > alignof(max_align_t)){ // TODO: Custom alignment
			return nullptr;
		}

		result = malloc(new_size);
		if(result){
			mem_zero(result, new_size);
		}
	break;

	case AllocatorMode_Realloc:
		if(align > alignof(max_align_t)){ // TODO: Custom alignment
			return nullptr;
		}

		result = realloc(ptr, new_size);
		if(result && (new_size > old_size)){
			uintptr diff = new_size - old_size;
			void* to_zero = (void*)(uintptr(result) + old_size);
			mem_zero(to_zero, diff);
		}
	break;

	case AllocatorMode_Free:
		free(ptr);
	break;

	case AllocatorMode_FreeAll:
		/* Unsupported */
	break;

	default: panic("Invalid mode");
	}

	return result;
}

Allocator heap_allocator(){
	return { heap_allocator_func, nullptr };
}
