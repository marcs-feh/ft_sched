#include "base.hpp"

#include "ft_sched.hpp"
#include "crc32.gen.cpp"

extern "C" {
	int printf(char const*, ...);
}

// - Task interface

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

template<typename T>
struct AddLValueReferenceImpl<T, true> { using Type = T&; };

template<typename T, auto = Referenceable<T>>
struct AddRValueReferenceImpl { using Type = T; };

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

// template<typename T>
// inline constexpr auto is_copy_constructible =
// 	__is_constructible(T, AddLValueReference<const T>)
// ;

// template<typename T>
// inline constexpr auto is_move_constructible =
// 	__is_constructible(T, AddRValueReference<T>)
// ;

template<class T>
void print_list(List<T> const& list, char const* elem_fmt){
	printf("len: %td cap: %td [ ", list.len, list.cap);
	for(usize i = 0; i < list.len; i ++){
		printf(elem_fmt, list[i]);
		printf(" ");
	} printf("]\n");
}

using TaskFunc = void (*)(void*);

enum TaskStatus : u16 {
	Undefined = 0,
	Initialized = 1,
	Started = 2,
	Done = 3,

	Fault, // Or anthing above
};

// struct RawTask {
// 	u32 id;
// 	void* arg;
// 	TaskFunc func;
// };

template<typename Fn>
struct Task {
	Arena* arena;
	Fn body;

	void run(){
		body();
	}

	Task() = delete;
	Task(Fn body) : body{body}{}
};

template<typename F>
concept Action = requires(F f){
	{ f() } -> SameAs<void>;
};

template<Action Fn> [[nodiscard]]
Task<Fn>* make_task(Arena* a, Fn body){
	auto t = make<Task<Fn>>(a, body);
	if(!t){ panic("Failed to create task"); }
	t->arena = a;
	return t;
}

int main(){
	usize arena_size = 4 * mem_kilobyte;
	u8* arena_data = new u8[arena_size];

	Arena arena = arena_from_buffer({arena_data, arena_size});
	
	auto q = make_sync_queue<f32>(&arena, 64);

	auto t = make_task(&arena, [=](){
		printf("Hello %p\n", q);
	});

	t->run();
}
