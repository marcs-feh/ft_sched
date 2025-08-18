#pragma once

#include <stddef.h>
#include <stdarg.h>
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

	Slice<T> slice() {
		return *this;
	}

	Slice<T> slice(usize start, usize end) {
		ensure(end <= this->len && end >= start, "Invalid slicing indices");
		return Slice<T>{ &this->data[start], end - start };
	}

	Slice<T> take(usize count) {
		ensure(count <= this->len, "Cannot take more than slice length");
		return Slice<T>{ this->data, this->count };
	}

	Slice<T> skip(usize count) {
		ensure(count <= this->len, "Cannot skip more than slice length");
		return Slice<T>{ &this->data[count], this->len - count };
	}

	T& operator[](usize idx){
		ensure(idx < len, "Out of bounds access");
		return data[idx];
	}

	T const& operator[](usize idx) const {
		ensure(idx < len, "Out of bounds access");
		return data[idx];
	}
};


//// Memory
constexpr usize mem_kilobyte = 1024ll;
constexpr usize mem_megabyte = 1024ll * 1024ll;
constexpr usize mem_gigabyte = 1024ll * 1024ll * 1024ll;

constexpr static inline
bool mem_valid_alignment(usize align){
	return align && ((align & (align - 1)) == 0);
}

uintptr mem_align_forward_ptr(uintptr p, uintptr a);

void* mem_copy(void* dest, void const* src, isize n);

void* mem_copy_no_overlap(void* dest, void const* src, isize n);

void* mem_zero(void* dest, isize n);

void* mem_set(void* dest, u8 v, isize n);

//// Arena
struct Arena {
	void* data;
	usize offset;
	usize capacity;
	void* last_allocation;
	i32   region_count;

	// Check if pointer is owned by arena
	bool owns(void* p);

	// Resize allocation in place, returns if it was successful
	bool resize(void* ptr, usize new_size);

	// Attempt to resize in place, otherwhise reallocate. Returns nullptr on failure
	void* realloc(void* ptr, usize old_size, usize new_size, usize align);

	// Allocate a block of memory from arena. Returns nullptr on failure
	void* alloc(usize size, usize align);

	// Reset arena, marking all allocations as free. This also ensures that there are not dangling regions.
	void reset();
};


// Initialize an arena from a buffer
Arena arena_from_buffer(Slice<u8> buf);

struct ArenaRegion {
	Arena* arena;
	usize  offset;
};

// Begin a temporary arena region, serving as a "checkpoint"
ArenaRegion arena_region_begin(Arena* a);

// End the region, restoring the arena state
void arena_region_end(ArenaRegion reg);

template<class T>
T* make(Arena* a){
	return (T*)a->alloc(sizeof(T), alignof(T));
}

template<class T>
Slice<T> make_slice(Arena* a, usize n){
	auto p = (T*)a->alloc(sizeof(T) * n, alignof(T));
	if(!p){ return Slice<T>{}; }
	return Slice<T>{p, n};
}

//// Dynamic Array
template<class T>
struct List {
	T*     data;
	usize  len;
	usize  cap;
	Arena* arena;

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
	T* new_data = (T*)arr->arena->realloc(arr->data, arr->cap * sizeof(T), new_cap * sizeof(T), alignof(T));
	if(!new_data){
		return false;
	}
	arr->data = new_data;
	arr->cap = new_cap;
	arr->len = min(arr->len, new_cap);
	return true;
}

constexpr usize LIST_GROWTH_FACTOR = 150;

template<class T>
bool append(List<T>* arr, T const& elem){
	if(arr->len >= arr->cap){
		usize new_cap = max<usize>(16, (arr->len * LIST_GROWTH_FACTOR) / 100);
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
		usize new_cap = max<usize>(16, (arr->len * LIST_GROWTH_FACTOR) / 100);
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
List<T> make_list(Arena* a, usize len, usize cap){
	auto p = (T*)a->alloc(sizeof(T) * cap, alignof(T));
	if(!p){ return List<T>{}; }
	return List<T>{p, len, cap, a};
}

template<class T>
List<T> make_list(Arena* a){
	return List<T>{nullptr, 0, 0, a};
}


//// Strings
struct String {
	char const* data;
	usize len;

	String slice();

	String slice(usize start, usize end);

	String take(usize count);

	String skip(usize count);

	String clone(Arena* arena);

	isize find(String sub, usize offset);

	// bool starts_with(String pattern);

	// bool ends_with(String pattern);

	// String trim(String cutset);
	// String trim_left(String cutset);
	// String trim_right(String cutset);

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


// String str_repeat(String str, usize count, Arena* a){
// 	String res = {0};
// 	char* buf = arena_make(a, char, count * str.len);
// 	if(!buf){ return res; }

// 	for(usize i = 0; i < count; i += 1){
// 		usize offset = i * str.len;
// 		mem_copy(&buf[offset], str.v, str.len);
// 	}

// 	res.v = buf;
// 	res.len = str.len * count;
// 	return res;
// }

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

cstring clone_to_cstring(String s, Arena* a);

String arena_vprintf(Arena* arena, char const* fmt, va_list args);

String arena_printf(Arena* arena, char const* fmt, ...);

