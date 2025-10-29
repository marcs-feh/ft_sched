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
static
int error_vprintf(char const* filename, int line, char const* fmt, va_list args){
	int n = 0;
	char buf[128] = {0};

	n += stbsp_snprintf(&buf[0], sizeof(buf), "(%s:%d) ", filename, line);
	n += stbsp_vsnprintf(&buf[n], sizeof(buf) - n, fmt, args);

	error_write(&buf[0]);
	return n;
}

static
int error_printf(char const* filename, int line, char const* fmt, ...){
	int n = 0;
	va_list args;
	va_start(args, fmt);
	n = error_vprintf(filename, line, fmt, args);
	va_end(args);
	return n;
}

void panic_ex(char const* msg, char const* filename, int line){
	error_printf(filename, line, "panic: %s", msg);
	do { trap(); } while(1);
}

bool ensure_ex(bool pred, char const* msg, char const* filename, int line){
	if(!pred){
		error_printf(filename, line, "Assertion failed: %s", msg);
		trap();
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
void Arena::reset(){
	ensure(this->region_count == 0, "Arena has dangling regions");
	this->offset = 0;
	this->last_allocation = nullptr;
}

bool Arena::owns(void* p){
	uintptr ptr = (uintptr)p;
	uintptr lo = (uintptr)this->data;
	uintptr hi = lo + this->capacity;

	return (ptr >= lo) && (ptr <= hi);
}

void* Arena::alloc(usize size, usize align){
	if(size == 0){ return nullptr; }
	uintptr base = (uintptr)this->data;
	uintptr current = base + (uintptr)this->offset;

	usize available = this->capacity - (current - base);

	uintptr aligned  = mem_align_forward_ptr(current, align);
	uintptr padding  = aligned - current;
	uintptr required = padding + size;

	if(required > available){
		return nullptr; /* Out of memory */
	}

	this->offset += required;
	void* allocation = (void*)aligned;
	this->last_allocation = allocation;
	mem_zero(allocation, size);

	return allocation;
}

void* Arena::realloc(void* ptr, usize old_size, usize new_size, usize align){
	if(ptr == nullptr){
		return this->alloc(new_size, align);
	}
	ensure(this->owns(ptr), "Pointer not owned by arena");

	bool in_place = this->resize(ptr, new_size);
	if(in_place){
		return ptr;
	}
	else {
		void* new_data = this->alloc(new_size, align);
		if(new_data == nullptr){ return nullptr; } /* Out of memory */
		mem_copy(new_data, ptr, min(old_size, new_size));

		if(new_size > old_size){
			usize diff = new_size - old_size;
			mem_zero((u8*)new_data + old_size, diff);
		}

		return new_data;
	}
}

bool Arena::resize(void* ptr, usize new_size){
	if(ptr == nullptr){ return false; }
	ensure(this->owns(ptr), "Pointer not owned by arena");

	uintptr base = (uintptr)this->data;

	if(ptr == this->last_allocation){
		uintptr last_alloc = (uintptr)this->last_allocation;

		if((last_alloc + new_size) > (base + this->capacity)){
			return false; /* No space left */
		}

		this->offset = (last_alloc + new_size) - base;
		// TODO: fill excess with 0s when increasing the allocation size
		return true;
	}

	return false;
}

Arena* Arena::make_sub(usize size){
	auto restore = this->offset;
	auto new_arena = (Arena*)this->alloc(sizeof(Arena), alignof(Arena));
	auto new_storage = (u8*)this->alloc(size, alignof(void*));

	if(new_arena == nullptr || new_storage == nullptr){
		this->offset = restore;
		return nullptr;
	}

	*new_arena = arena_from_buffer({new_storage, size});
	return new_arena;
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

//// String
String string_clone(String s, Arena* a){
	String res = {};
	auto buf = make_slice<u8>(a, s.len);
	if(buf.data == nullptr){ return res; }
	mem_copy_no_overlap(buf.data, s.data, s.len);

	res.data = (char const*)buf.data;
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

isize String::find(String sub, usize offset){
	if(sub.len > this->len){ return -1; }
	ensure(offset <= this->len, "Invalid search offset");

	for(usize i = offset; i <= (this->len - sub.len); i += 1){
		if(mem_compare(&this->data[i], sub.data, sub.len) == 0){
			return i;
		}
	}

	return -1;
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

String buffer_vprintf(Slice<u8> buf, char const* fmt, va_list args){
	int n = stbsp_vsnprintf((char*)buf.data, (int)buf.len, fmt, args);
	if(n > 0){
		return String((char const*)buf.data, n);
	}
	return {};
}

String buffer_printf(Slice<u8> buf, char const* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	String res = buffer_vprintf(buf, fmt, args);
	va_end(args);
	return res;
}

String arena_vprintf(Arena* arena, char const* fmt, va_list args){
	u8* base = (u8*)((uintptr)arena->data + arena->offset);
	usize size = arena->capacity - arena->offset;

	String res = buffer_vprintf({base, size}, fmt, args);
	arena->offset += res.len;

	return res;
}

String arena_printf(Arena* arena, char const* fmt, ...){
	va_list args;
	va_start(args, fmt);
	String res = arena_vprintf(arena, fmt, args);
	va_end(args);
	return res;
}


//// Heap
// extern "C"{
// 	void* malloc(size_t);
// 	void* realloc(void*, size_t);
// 	void free(void*);
// }

//// Spinlock
void Spinlock::lock(){
	while(1){
		if(!this->_state.exchange(true, std::memory_order_acquire)){
			break;
		}
		while(this->_state.load(std::memory_order_relaxed));
	}
}

void Spinlock::unlock(){
	this->_state.store(false, std::memory_order_release);
}

bool Spinlock::try_lock(){
    return !_state.load(std::memory_order_relaxed)
    	&& !_state.exchange(true, std::memory_order_acquire);
}

