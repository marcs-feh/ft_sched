#pragma once

//// Attributes and Compiler specifics
#if defined(_MSC_VER)
	#define attribute_force_inline __forceinline
#elif defined(__clang__) || defined(__GNUC__)
	#define attribute_force_inline __attribute__((always_inline))
#else
	#warning "Could not find force_inline attribute. This may degrade performance"
	#define attribute_force_inline
#endif

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <atomic>

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

template<class T>
using Atomic = std::atomic<T>;

constexpr auto memory_order_relaxed = std::memory_order_relaxed;
constexpr auto memory_order_acquire = std::memory_order_acquire;
constexpr auto memory_order_release = std::memory_order_release;
constexpr auto memory_order_seq_cst = std::memory_order_seq_cst;
// NOTE: `consume` memory order has been deliberately ommited due to being poorly specified

using AtomicI8  = Atomic<i8>;
using AtomicI16 = Atomic<i16>;
using AtomicI32 = Atomic<i32>;
using AtomicI64 = Atomic<i64>;

using AtomicU8  = Atomic<u8>;
using AtomicU16 = Atomic<u16>;
using AtomicU32 = Atomic<u32>;
using AtomicU64 = Atomic<u64>;

using AtomicUsize = Atomic<usize>;
using AtomicIsize = Atomic<isize>;

using AtomicBool = Atomic<bool>;

static_assert(AtomicI8::is_always_lock_free, "Expected i8 to be lock-free");
static_assert(AtomicI16::is_always_lock_free, "Expected i16 to be lock-free");
static_assert(AtomicI32::is_always_lock_free, "Expected i32 to be lock-free");
static_assert(AtomicI64::is_always_lock_free, "Expected i64 to be lock-free");
static_assert(AtomicU8::is_always_lock_free, "Expected u8 to be lock-free");
static_assert(AtomicU16::is_always_lock_free, "Expected u16 to be lock-free");
static_assert(AtomicU32::is_always_lock_free, "Expected u32 to be lock-free");
static_assert(AtomicU64::is_always_lock_free, "Expected u64 to be lock-free");
static_assert(AtomicUsize::is_always_lock_free, "Expected usize to be lock-free");
static_assert(AtomicIsize::is_always_lock_free, "Expected isize to be lock-free");
static_assert(AtomicBool::is_always_lock_free, "Expected bool to be lock-free");

//// Type traits

// Type tag used to differentiate operator new overloads and to ensure valid union active members
struct Nat {};

// Custom tagged operator new overload, to avoid clashes with other library defs and not need to include <new>
inline void *operator new(decltype(sizeof 0), void* ptr, Nat) {
	return ptr;
}

template<typename T>
struct Identity { using Type = T; };

template<typename T>
concept Referenceable = requires {
	typename Identity<T&>;
};

namespace detail {
template<typename T> struct RemoveReferenceImpl      { using Type = T; };
template<typename T> struct RemoveReferenceImpl<T&>  { using Type = T; };
template<typename T> struct RemoveReferenceImpl<T&&> { using Type = T; };

// Add references to types
// NOTE: auto = ... evaluates to true or false, so we only add references to referenceable types

template<typename T, auto = Referenceable<T>>
struct AddLValueReferenceImpl { using Type = T; };

// Specialization to only add l-value reference for Referenceable types
template<typename T>
struct AddLValueReferenceImpl<T, true> { using Type = T&; };

template<typename T, auto = Referenceable<T>>
struct AddRValueReferenceImpl { using Type = T; };

// Specialization to only add r-value reference for Referenceable types
template<typename T>
struct AddRValueReferenceImpl<T, true> { using Type = T&&; };
}

template<typename T>
using RemoveReference = typename detail::RemoveReferenceImpl<T>::Type;

template<typename T>
using AddLValueReference = typename detail::AddLValueReferenceImpl<T>::Type;

template<typename T>
using AddRValueReference = typename detail::AddRValueReferenceImpl<T>::Type;

template<typename A, typename B>
inline constexpr auto same_type = false;

template<typename T>
inline constexpr auto same_type<T, T> = true;

template<typename A, typename B>
concept SameAs = same_type<A, B>;

template<typename From, typename To>
concept ConvertibleTo = requires(From x){
	{ static_cast<To>(x) };
};

template<typename T> attribute_force_inline constexpr
RemoveReference<T>&& move(T&& arg) {
	return static_cast<RemoveReference<T>&&>(arg);
}

template<typename T> constexpr
T&& forward(RemoveReference<T>&& arg) {
	return static_cast<T&&>(arg);
}

template<typename T> constexpr
T&& forward(RemoveReference<T>& arg) {
	return static_cast<T&&>(arg);
}

template<typename T, typename U = T> constexpr
T exchange(T& x, U&& v){
	T r = move(x);
	x = forward<U>(v);
	return r;
}


//// Defer
namespace defer_detail {
template<typename F>
struct DeferredCall {
	F f;

	attribute_force_inline DeferredCall(F&& f) : f(move(f)){}
	
	attribute_force_inline ~DeferredCall(){ f(); }
};

template<typename F> attribute_force_inline
auto make_deferred(F&& f){
	return DeferredCall<F>(forward<F>(f));
}

#define DEFER_GLUE0(X, Y) X##Y
#define DEFER_GLUE1(X, Y) DEFER_GLUE0(X, Y)
#define DEFER_GLUE2(X, Y) DEFER_GLUE1(X, Y)
#define DEFER_COUNTER(X)  DEFER_GLUE2(X, __COUNTER__)
}

#define defer(STMT) auto DEFER_COUNTER(_defer_expr_) = ::defer_detail::make_deferred([&](){ STMT; })

//// Assertions
[[noreturn]] void panic_ex(char const* msg, char const* filename, int line);

bool ensure_ex(bool pred, char const* msg, char const* filename, int line);

#define ensure(Pred, Msg) ensure_ex((Pred), (Msg), __FILE__, __LINE__)
#define panic(Msg) panic_ex((Msg), __FILE__, __LINE__)
#define unimplemented() panic_ex("Unimplemented", __FILE__, __LINE__)

//// Option
template<typename T>
struct Option {
	union {
		T _value;
		Nat _nat;
	};
	bool _has_value = false;

	attribute_force_inline constexpr auto ok() const { return _has_value; }

	[[nodiscard]] constexpr
	T unwrap(){
		if(!_has_value){
			panic("unwrap() on empty option");
		}
		auto v = move(_value);
		drop();
		return v;
	}

	[[nodiscard]] constexpr
	T unwrap_unchecked(){
		auto v = move(_value);
		drop();
		return v;
	}

	template<typename U>
	T unwrap_or(U&& alt){
		if(!_has_value){
			return forward<U>(alt);
		}
		else {
			return unwrap();
		}
	}

	attribute_force_inline constexpr
	Option<T>* drop(){
		if(_has_value){
			_value.~T();
		}
		_has_value = false;
		return this;
	}

	constexpr
	Option() : _nat{}, _has_value{false} {}

	constexpr
	Option(T&& v)
		: _value{move(v)}
		, _has_value{true} {}

	constexpr
	Option(Option<T>&& other)
		: _nat{}
		, _has_value{exchange(other._has_value, false)}
	{
		if(_has_value){
			new (&_value, Nat{}) T{move(other._value)};
			other.drop();
		}
	}

	constexpr
	Option<T>& operator=(T&& v){
		return *new(drop(), Nat{}) Option{move(v)};
	}

	constexpr
	Option<T>& operator=(Option<T>&& o){
		return *new(drop(), Nat{}) Option{move(o)};
	}

	~Option(){
		drop();
	}
};

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
		return Slice<T>{ this->data, count };
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

	// Allocate a zeroed block of memory from arena. Returns nullptr on failure
	void* alloc(usize size, usize align);

	// Allocate a sub-arena
	Arena* make_sub(usize size);

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

template<class T, typename ... Args> [[nodiscard]]
T* make(Arena* a, Args&& ... args){
	T* p = (T*)a->alloc(sizeof(T), alignof(T));
	if(!p){ return nullptr; }
	new (p, Nat{}) T(forward<Args>(args)...);
	return p;
}

template<class T, typename ... Args> [[nodiscard]]
T* make_uninitialized(Arena* a){
	T* p = (T*)a->alloc(sizeof(T), alignof(T));
	return p;
}

template<class T> [[nodiscard]]
Slice<T> make_slice(Arena* a, usize n){
	auto p = (T*)a->alloc(sizeof(T) * n, alignof(T));
	if(!p){ return Slice<T>{}; }
	for(usize i = 0; i < n; i++){
		new (&p[i], Nat{}) T{};
	}
	return Slice<T>{p, n};
}

//// Dynamic Array
constexpr usize LIST_GROWTH_FACTOR = 150;

template<class T>
struct List {
	T*     data;
	usize  len;
	usize  cap;
	Arena* arena;

	bool resize(usize new_cap){
		T* new_data = (T*)this->arena->realloc(this->data, this->cap * sizeof(T), new_cap * sizeof(T), alignof(T));
		if(!new_data){
			return false;
		}
		this->data = new_data;
		// TODO: construct newly created shit when newcap > len
		this->cap = new_cap;
		this->len = min(this->len, new_cap);
		return true;
	}

	bool append(T const& elem){
		if(this->len >= this->cap){
			usize new_cap = max<usize>(16, (this->len * LIST_GROWTH_FACTOR) / 100);
			if(!this->resize(new_cap)){
				return false;
			}
		}

		new (&this->data[this->len], Nat{}) T(elem);
		this->len += 1;
		return true;
	}

	bool pop(){
		if(this->len == 0){
			return false;
		}

		this->data[this->len - 1].~T();
		this->len -= 1;
		return true;
	}

	bool pop(T* elem){
		if(this->len == 0){
			return false;
		}

		this->len -= 1;
		*elem = ::move(this->data[this->len]);
		return true;
	}

	bool insert(T const& elem, usize idx){
		ensure(idx <= this->len, "Out of bounds insertion");

		if(this->len >= this->cap){
			usize new_cap = max<usize>(16, (this->len * LIST_GROWTH_FACTOR) / 100);
			if(!this->resize(new_cap)){
				return false;
			}
		}

		mem_copy(&this->data[idx + 1], &this->data[idx], sizeof(T) * (this->len - idx));
		new (&this->data[idx], Nat{}) T(elem);
		this->len += 1;
		return true;
	}

	bool remove(usize idx){
		ensure(idx < this->len, "Out of bounds deletion");
		if(this->len == 0){
			return false;
		}

		this->data[idx].~T();

		mem_copy(&this->data[idx], &this->data[idx + 1], sizeof(T) * (this->len - idx));
		this->len -= 1;
		return true;
	}

	Slice<T> slice() {
		return Slice<T>{this->data, this->len};
	}

	Slice<T> slice(usize start, usize end) {
		ensure(end <= this->len && end >= start, "Invalid slicing indices");
		return Slice<T>{ &this->data[start], end - start };
	}

	Slice<T> take(usize count) {
		ensure(count <= this->len, "Cannot take more than List length");
		return Slice<T>{ this->data, count };
	}

	Slice<T> skip(usize count) {
		ensure(count <= this->len, "Cannot skip more than slice length");
		return Slice<T>{ &this->data[count], this->len - count };
	}

	T& operator[](usize idx) {
		ensure(idx < len, "Out of bounds list access");
		return data[idx];
	}

	T const& operator[](usize idx) const {
		ensure(idx < len, "Out of bounds list access");
		return data[idx];
	}

	void clear(){
		for(usize i = 0; i < this->len; i += 1){
			this->data[i].~T();
		}
		this->len = 0;
	}

	~List(){
		clear();
	}
};


template<class T> [[nodiscard]]
List<T> make_list(Arena* a, usize len, usize cap){
	auto p = (T*)a->alloc(sizeof(T) * cap, alignof(T));
	if(!p){ return List<T>{}; }
	return List<T>{p, len, cap, a};
}

template<class T> [[nodiscard]]
List<T> make_list(Arena* a){
	return List<T>{nullptr, 0, 0, a};
}

//// Statically sized list
// template<class T, usize N>
// struct SmallList {
// 	T      data[N]{};
// 	usize  len = 0;

// 	// bool resize(usize new_cap){
// 	// 	if(new_cap <= N){
// 	// 		if(new_cap > N){

// 	// 		}
// 	// 		this->len = min(this->len, new_cap);
// 	// 	}
// 	// 	return true;
// 	// }

// 	bool append(T const& elem){
// 		if(this->len >= N){
// 			return false;
// 		}

// 		this->data[this->len] = elem;
// 		this->len += 1;
// 		return true;
// 	}

// 	bool pop(){
// 		if(this->len == 0){
// 			return false;
// 		}

// 		this->len -= 1;
// 		return true;
// 	}

// 	bool pop(T* elem){
// 		if(this->len == 0){
// 			return false;
// 		}

// 		this->len -= 1;
// 		*elem = this->data[this->len];
// 		return true;
// 	}

// 	bool insert(T const& elem, usize idx){
// 		ensure(idx <= this->len, "Out of bounds insertion");

// 		if(this->len >= N){
// 			return false;
// 		}
// 		mem_copy(&this->data[idx + 1], &this->data[idx], sizeof(T) * (this->len - idx));

// 		new (&this->data[this->len], Nat{}) T(elem);
// 		this->len += 1;
// 		return true;
// 	}

// 	bool remove(usize idx){
// 		ensure(idx < this->len, "Out of bounds deletion");
// 		if(this->len == 0){
// 			return false;
// 		}
// 		mem_copy(&this->data[idx], &this->data[idx + 1], sizeof(T) * (this->len - idx));
// 		this->len -= 1;
// 		return true;
// 	}

// 	Slice<T> slice() {
// 		return Slice<T>{this->data, this->len};
// 	}

// 	Slice<T> slice(usize start, usize end) {
// 		ensure(end <= this->len && end >= start, "Invalid slicing indices");
// 		return Slice<T>{ &this->data[start], end - start };
// 	}

// 	Slice<T> take(usize count) {
// 		ensure(count <= this->len, "Cannot take more than List length");
// 		return Slice<T>{ this->data, count };
// 	}

// 	Slice<T> skip(usize count) {
// 		ensure(count <= this->len, "Cannot skip more than slice length");
// 		return Slice<T>{ &this->data[count], this->len - count };
// 	}

// 	T& operator[](usize idx) {
// 		ensure(idx < len, "Out of bounds list access");
// 		return data[idx];
// 	}

// 	T const& operator[](usize idx) const {
// 		ensure(idx < len, "Out of bounds list access");
// 		return data[idx];
// 	}
// };


//// Strings
struct String {
	char const* data;
	usize len;

	String slice();

	String slice(usize start, usize end);

	String take(usize count);

	String skip(usize count);

	Slice<u8> raw_bytes();

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

String buffer_vprintf(Slice<u8> buf, char const* fmt, va_list args);

String arena_vprintf(Arena* arena, char const* fmt, va_list args);

String arena_printf(Arena* arena, char const* fmt, ...);

//// Spinlock
struct Spinlock {
	Atomic<bool> _state{0};

	void lock();

	void unlock();

	bool try_lock();
};

//// SPSC queue
constexpr usize destructive_interference_size = 64;

template<class T>
struct SPSC_Queue {
	T* data;
	usize capacity; // NOTE: Remember that there's always a "slack" slot, so the effective capacity is actually capacity - 1
	alignas(destructive_interference_size)
	Atomic<usize> read_pos;
	alignas(destructive_interference_size)
	Atomic<usize> write_pos;

	template<typename ...Args>
	bool try_emplace(Args&& ...args){
		auto cur_write_pos = this->write_pos.load(memory_order_relaxed);
		auto next_write_pos = (cur_write_pos + 1) % capacity;

		if(next_write_pos != this->read_pos.load(memory_order_acquire)){
			new(&this->data[write_pos], Nat{}) T{forward<Args>(args)...};

			this->write_pos.store(next_write_pos, memory_order_release);
			return true;
		}

		return false;
	}

	template<ConvertibleTo<T> U>
	bool try_push(U&& elem){
		return try_emplace(forward<U>(elem));
	}

	template<ConvertibleTo<T> U>
	void push(U&& elem){
		auto ok = try_emplace(forward<U>(elem));
		while(!ok){
			ok = try_push(forward<U>(elem));
		}
	}

	Option<T> pop(){
		auto cur_read_pos = this->read_pos.load(memory_order_relaxed);
		if(cur_read_pos == this->write_pos.load(memory_order_acquire)){
			return {};
		}

		auto elem = Option<T>{move(this->data[cur_read_pos])};

		this->data[cur_read_pos].~T();

		auto next_read_pos = (cur_read_pos + 1) % capacity;
		this->read_pos.store(next_read_pos, memory_order_release);
		return elem;
	}

	bool pop_into(T* out){
		auto elem = pop();
		auto ok = elem._has_value;
		if(ok){
			*out = elem.unwrap_unchecked();
		}
		return ok;
	}

	SPSC_Queue(SPSC_Queue const&) = delete;

	SPSC_Queue(SPSC_Queue&& q) = delete;

	SPSC_Queue()
		: data{nullptr}
		, capacity{0}
		, read_pos{0}
		, write_pos{0} {}
};

template<typename T>
SPSC_Queue<T>* make_spsc_queue(Arena* a, usize capacity){
	auto restore = a->offset;
	auto queue = make<SPSC_Queue<T>>(a);
	auto data = (T*)a->alloc(sizeof(T) * capacity, alignof(T));

	queue->data = data;
	queue->capacity = capacity;
	if(!queue || !data){
		a->offset = restore;
		return nullptr;
	}

	return queue;
}

