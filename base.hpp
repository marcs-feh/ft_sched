#pragma once

#include <stddef.h>
#include <stdint.h>

//// Basic types & Utilities
using i8 = int8_t;
using u8 = uint8_t;

using i16 = int16_t;
using u16 = uint16_t;

using i32 = int32_t;
using u32 = uint32_t;

using i64 = int64_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

using rune = int32_t;
using uintptr = uintptr_t;

using usize = size_t;
using isize = ptrdiff_t;

using cstring = const char *;

usize cstring_len(cstring cs);

template<class T> constexpr
T min(T x, T y){
	return x < y ? x : y;
}

template<class T> constexpr
T max(T x, T y){
	return x > y ? x : y;
}

template<class T> constexpr
T clamp(T lo, T x, T hi){
	return min(max(lo, x), hi);
}

//// Assertions
[[noreturn]] void panic_ex(char const* msg, char const* filename, int line);

bool ensure_ex(bool pred, char const* msg, char const* filename, int line);

#define ensure(Pred, Msg) ensure_ex((Pred), (Msg), __FILE__, __LINE__)
#define panic(Msg) panic_ex((Msg), __FILE__, __LINE__)
#define unimplemented() panic_ex("Unimplemented", __FILE__, __LINE__)

//// Slice
template<class T>
struct Slice {
	T* data;
	usize len;

	T& operator[](usize idx){
		ensure(idx < len, "Out of bounds access");
		return data[idx];
	}

	T const& operator[](usize idx) const {
		ensure(idx < len, "Out of bounds access");
		return data[idx];
	}
};

template<class T>
Slice<T> slice(Slice<T> s) {
	return s;
}

template<class T>
Slice<T> slice(Slice<T> s, usize start, usize end) {
	ensure(end <= s.len && end >= start, "Invalid slicing indices");
	return Slice<T>{ &s.data[start], end - start };
}

template<class T>
Slice<T> take(Slice<T> s, usize count) {
	ensure(count <= s.len, "Cannot take more than slice length");
	return Slice<T>{ s.data, s.count };
}

template<class T>
Slice<T> skip(Slice<T> s, usize count) {
	ensure(count <= s.len, "Cannot skip more than slice length");
	return Slice<T>{ &s.data[count], s.len - count };
}

//// Memory
void* mem_copy(void* dest, void const* src, isize n);

void* mem_copy_no_overlap(void* dest, void const* src, isize n);

void* mem_set(void* dest, u8 v, isize n);

void* mem_zero(void* dest, isize n);

isize mem_compare(void const* lhs, void const* rhs, isize n);

constexpr static inline
bool mem_valid_alignment(usize align){
	return align && ((align & (align - 1)) == 0);
}

uintptr mem_align_forward_ptr(uintptr p, uintptr a);

enum AllocatorMode : u8 {
	AllocatorMode_Alloc = 0,
	AllocatorMode_Free,
	AllocatorMode_Realloc,
	AllocatorMode_FreeAll,
};

using AllocatorFunc = void* (*) (void* data, AllocatorMode mode, void* ptr, usize old_size, usize new_size, usize align);

struct Allocator {
	AllocatorFunc func;
	void* data;
};

static inline
void* mem_alloc(Allocator a, usize size, usize align){
	return a.func(a.data, AllocatorMode_Alloc, nullptr, 0, size, align);
}

static inline
void* mem_realloc(Allocator a, void* ptr, usize old_size, usize new_size, usize align){
	if(ptr == nullptr){
		return mem_alloc(a, new_size, align);
	}
	return a.func(a.data, AllocatorMode_Realloc, ptr, old_size, new_size, align);
}

static inline
void mem_free(Allocator a, void* ptr, usize size, usize align){
	if(ptr == nullptr){ return; }
	a.func(a.data, AllocatorMode_Free, ptr, size, 0, align);
}

static inline
void mem_free_all(Allocator a){
	a.func(a.data, AllocatorMode_FreeAll, nullptr, 0, 0, 0);
}

template<class T>
T* make(Allocator a){
	return (T*)mem_alloc(a, sizeof(T), alignof(T));
}

template<class T>
Slice<T> make_slice(Allocator a, usize n){
	auto p = (T*)mem_alloc(a, sizeof(T) * n, alignof(T));
	if(!p){ return Slice<T>{}; }
	return Slice<T>{p, n};
}

//// Dynamic Array
template<class T>
struct List {
	T*        data;
	usize     len;
	usize     cap;
	Allocator allocator;

	T& operator[](usize idx) {
		ensure(idx < len, "Out of bounds list access");
		return data[idx];
	}

	T const& operator[](usize idx) const {
		ensure(idx < len, "Out of bounds list access");
		return data[idx];
	}
};

template<class T>
bool resize(List<T>* arr, usize new_cap){
	T* new_data = (T*)mem_realloc(arr->allocator, arr->data, arr->cap * sizeof(T), new_cap * sizeof(T), alignof(T));
	if(!new_data){
		return false;
	}
	arr->data = new_data;
	arr->cap = new_cap;
	arr->len = min(arr->len, new_cap);
	return true;
}

constexpr usize ARRAY_GROWTH_FACTOR = 150;

template<class T>
bool append(List<T>* arr, T const& elem){
	if(arr->len >= arr->cap){
		usize new_cap = max<usize>(16, (arr->len * ARRAY_GROWTH_FACTOR) / 100);
		if(!resize(arr, new_cap)){
			return false;
		}
	}

	arr->data[arr->len] = elem;
	arr->len += 1;
	return true;
}

template<class T>
bool pop(List<T>* arr){
	if(arr->len == 0){
		return false;
	}

	arr->len -= 1;
	return true;
}

template<class T>
bool pop(List<T>* arr, T* elem){
	if(arr->len == 0){
		return false;
	}

	arr->len -= 1;
	*elem = arr->data[arr->len];
	return true;
}

template<class T>
bool insert(List<T>* arr, T const& elem, usize idx){
	ensure(idx <= arr->len, "Out of bounds insertion");

	if(arr->len >= arr->cap){
		usize new_cap = max<usize>(16, (arr->len * ARRAY_GROWTH_FACTOR) / 100);
		if(!resize(arr, new_cap)){
			return false;
		}
	}
	mem_copy(&arr->data[idx + 1], &arr->data[idx], sizeof(T) * (arr->len - idx));
	arr->data[idx] = elem;
	arr->len += 1;
	return true;
}

template<class T>
bool remove(List<T>* arr, usize idx){
	ensure(idx < arr->len, "Out of bounds deletion");
	if(arr->len == 0){
		return false;
	}
	mem_copy(&arr->data[idx], &arr->data[idx + 1], sizeof(T) * (arr->len - idx));
	arr->len -= 1;
	return true;
}

template<class T>
Slice<T> slice(List<T> const& s) {
	return Slice<T>{s.data, s.len};
}

template<class T>
Slice<T> slice(List<T> const& s, usize start, usize end) {
	ensure(end <= s.len && end >= start, "Invalid slicing indices");
	return Slice<T>{ &s.data[start], end - start };
}

template<class T>
Slice<T> take(List<T> const& s, usize count) {
	ensure(count <= s.len, "Cannot take more than List length");
	return Slice<T>{ s.data, s.count };
}

template<class T>
Slice<T> skip(List<T> const& s, usize count) {
	ensure(count <= s.len, "Cannot skip more than slice length");
	return Slice<T>{ &s.data[count], s.len - count };
}

template<class T>
List<T> make_list(Allocator a, usize len, usize cap){
	auto p = (T*)mem_alloc(a, sizeof(T) * cap, alignof(T));
	if(!p){ return List<T>{}; }
	return List<T>{p, len, cap, a};
}

template<class T>
List<T> make_list(Allocator a){
	return List<T>{nullptr, 0, 0, a};
}

//// Arena
struct Arena {
	void* data;
	usize offset;
	usize capacity;
	void* last_allocation;
	i32   region_count;
};

struct ArenaRegion {
	Arena* arena;
	usize  offset;
};

// Initialize an arena from a buffer
Arena arena_from_buffer(Slice<u8> buf);

// Check if pointer is owned by arena
bool arena_owns(Arena* a, void* p);

// Resize allocation in place, returns if it was successful
bool arena_resize(Arena* a, void* ptr, usize new_size);

// Attempt to resize in place, otherwhise reallocate. Returns nullptr on failure
void* arena_realloc(Arena* a, void* ptr, usize old_size, usize new_size, usize align);

// Allocate a block of memory from arena. Returns nullptr on failure
void* arena_alloc(Arena* a, usize size, usize align);

// Reset arena, marking all allocations as free. This also ensures that there are not dangling regions.
void arena_reset(Arena* a);

// Begin a temporary arena region, serving as a "checkpoint"
ArenaRegion arena_region_begin(Arena* a);

// End the region, restoring the arena state
void arena_region_end(ArenaRegion reg);

// Get arena in a Allocator conforming interface
Allocator arena_allocator(Arena* a);

template<class T>
T* make(Arena* arena){
	return (T*)arena_alloc(arena, sizeof(T), alignof(T));
}

template<class T>
Slice<T> make_slice(Arena* arena, usize count){
	auto p = (T*)arena_alloc(arena, sizeof(T) * count, alignof(T));
	if(!p){
		return Slice<T>{nullptr,0};
	}
	return Slice<T>{p, count};
}

//// Heap
Allocator heap_allocator();

//// Strings
struct String {
	char const* data;
	usize len;

	u8 operator[](usize idx) const {
		ensure(idx < len, "Out of bounds access");
		return data[idx];
	}

	bool operator==(String s) const {
		return !(*this != s);
	}

	bool operator!=(String s) const {
		if(len != s.len){ return true; }

		for(usize i = 0; i < len; i += 1){
			if(data[i] != s.data[i]){ return true; }
		}

		return false;
	}

	String() : data{0}, len{0} {}

	String(cstring cs) : data{cs}, len{cstring_len(cs)} {}

	String(char const* p, usize n) : data{p}, len{n} {}

	explicit String(Slice<u8> s) : data{(char const*)s.data}, len{s.len} {}
};

String slice(String s);

String slice(String s, usize start, usize end);

String take(String s, usize count);

String skip(String s, usize count);

#define str_fmt(S) ((int)((S).len)), ((char const*)((S).data))

constexpr rune RUNE_ERROR = 0xfffd;

struct RuneDecoded {
	rune codepoint;
	u32  size;
};

struct RuneEncoded {
	u8  bytes[4];
	u32 size;
};

RuneEncoded rune_encode(rune r);

RuneDecoded rune_decode(u8 const* buf, u32 buflen);

String clone(Arena* arena, String s);

cstring clone_to_cstring(String s, Arena* a);

String str_vprintf(Arena* arena, char const* fmt, va_list args);

String str_printf(Arena* arena, char const* fmt, ...);
