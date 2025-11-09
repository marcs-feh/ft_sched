#pragma once
#include "base.hpp"
//// Compiler intrinsics

#if defined(__clang__) || defined(__GNUC__)
	// Cause a full memory clobber, this emits no CPU instructions but prevents the compiler from doing optimizing away certain loads
	#define COMPILER_MEMORY_BARRIER() asm volatile("" : : : "memory")
#else
	#error "Unsupported compiler, need to have a COMPILER_MEMORY_BARRIER macro"
#endif

//// Timing
using TimeTick = usize;

struct Duration {
	static constexpr isize scale = 1'000'000;

	static constexpr isize second = scale / 1;
	static constexpr isize millisecond = scale / 1'000;
	static constexpr isize microsecond = scale / 1'000'000;

	i64 _value{0};

	constexpr static Duration from_second(isize n){
		return {(n * scale) / 1};
	}

	constexpr static Duration from_milli(isize n){
		isize val = {(n * scale) / 1'000};
		return Duration{val};
	}

	constexpr static Duration from_micro(isize n){
		return {(n * scale) / 1'000'000};
	}

	constexpr isize to_second() const {
		return _value / second;
	}

	constexpr isize to_milli() const {
		return _value / millisecond;
	}

	constexpr isize to_micro() const {
		return _value / microsecond;
	}

	#define X(Op) constexpr Duration operator Op(Duration d) const { return { _value Op d._value }; }
		X(+) X(-) X(*) X(/)
	#undef X

	#define X(Op) constexpr bool operator Op(Duration d) const { return this->_value Op d._value; }
		X(<) X(>) X(<=) X(>=) X(==) X(!=)
	#undef X
};

TimeTick tick_now();

usize tick_frequency();

static inline
Duration tick_diff(TimeTick a, TimeTick b){
	i64 diff = i64(a) - i64(b);
	auto time_diff = static_cast<isize>((diff * i64(Duration::scale)) / i64(tick_frequency()));
	return {time_diff};
}

void sleep_for(Duration d);

//// Tasks
enum TaskStatus : u8 {
	TaskStatus_Undefined = 0,
	TaskStatus_Initialized = 1,
	TaskStatus_Started = 2,
	TaskStatus_Done = 3,

	TaskStatus_Fault, // Or anthing above
};

struct RawTask;

// Yield the currently executing task
void task_yield();

//// Deadlines

using SlotCancellationCallback = void (*) (void* data);

struct DeadlineSlot {
	TimeTick last_tick;
	Duration limit;
	void* key;
	SlotCancellationCallback cancel;

	void reset(){
		last_tick = tick_now();
	}

	DeadlineSlot()
		: last_tick{0}
		, limit{0}
		, key{nullptr}
		, cancel{nullptr}
	{}
};

struct DeadlineWatcher {
	Slice<DeadlineSlot> slots;
	Spinlock _lock{};
	Atomic<u32> _count;
	char name[12] = {};

	auto lock_guard(){
		return _lock.guard();
	}

	usize count() const {
		return _count.load(memory_order_relaxed);
	}

	[[nodiscard]]
	bool add(void* key, SlotCancellationCallback cancel, Duration limit);

	bool reset_deadline(void* key);

	DeadlineSlot* get(void* key);

	void remove(DeadlineSlot* node);

	void remove_key(void* key);

	void clear();

	bool scan();

	void _remove_no_lock(DeadlineSlot* node);

	DeadlineWatcher()
		: slots{}
		, _lock{}
	{
	}
};

void init_deadline_watcher(DeadlineWatcher* w, Slice<DeadlineSlot> slots);

DeadlineWatcher* make_deadline_watcher(Arena* a, usize slot_count);

// Task object that closely maps to a regular thread
using RawTaskFunc = void (*)(RawTask* t);

struct RawTaskPlatformSpecificData {
	alignas(alignof(void*) * 2) char data[16];
};

using TaskCancelCallback = void (*)(RawTask* self);

constexpr usize task_stack_alignment = max<usize>(alignof(void*), 16);

constexpr usize task_min_stack_size = 200;

struct RawTask {
	RawTaskFunc func = nullptr;
	Arena* arena = nullptr;
	void* args = nullptr;
	u32 stack_size = 0;
	DeadlineWatcher* supervisor = nullptr;
	u32 args_size = 0;
	u32 id{};
	Atomic<TaskStatus> _status = TaskStatus_Initialized;

	// Optional callback to unlock mutexes or release any additional resource after killing the thread
	TaskCancelCallback on_cancel = nullptr;

	RawTaskPlatformSpecificData _specific{};

	TaskStatus status() {
		return _status;
	}

	void join(CALLER_LOCATION) {
		auto ok = _platform_join();
		if(!ok){
			error_printf(caller_location.file_name(), caller_location.line(),
				"[Task %2d] Failed to join task (%p)\r\n", int(id), this
			);
		}
		arena->reset();
	}

	void cancel(CALLER_LOCATION){
		auto ok = _platform_cancel();

		if(!ok){
			error_printf(caller_location.file_name(), caller_location.line(),
				"[Task %2d] Failed to cancel task (%p)\r\n", int(id), this
			);
		}
		arena->reset();
	}

	[[nodiscard]]
	bool attach_supervisor(DeadlineWatcher* watcher, Duration limit);

	~RawTask(){}

	bool _platform_init(Arena* a, usize stack_size, RawTaskFunc func, void* args);
	bool _platform_join();
	bool _platform_cancel();
};

u32 next_raw_task_id();

bool init_raw_task(RawTask* task, Arena* parent, usize sub_arena_size, usize stack_size, RawTaskFunc func, void* args);

RawTask* make_raw_task(Arena* parent, usize sub_arena_size, usize stack_size, RawTaskFunc func, void* args);


template<typename Impl, typename T>
concept Task = requires(Impl impl){
	{ impl.result() } -> SameAs<Option<T>>;
	{ impl.status() } -> SameAs<TaskStatus>;
	{ impl.cancel() } -> SameAs<void>;
	{ impl.join() } -> SameAs<void>;
	{ impl.id() } -> SameAs<u32>;
	// { impl.raw_task() } -> SameAs<RawTask*>;
};

template<typename F, typename Output, typename ... Args>
concept Callable = requires(F f, Args... args){
	{ f(args...) } -> SameAs<Output>;
};

struct TaskContext {
	RawTask* task{nullptr};

	void ensure(bool pred, cstring msg, CALLER_LOCATION){
		if(!pred){
			error_printf(caller_location.file_name(), caller_location.line(),
				"[Task %d] Assertion failed: %s\r\n",
				int(task->id), msg
			);
			task->cancel();
		}
	}

	void panic(cstring msg, CALLER_LOCATION){
		error_printf(caller_location.file_name(), caller_location.line(),
			"[Task %d] Panic: %s\r\n",
			int(task->id), msg
		);
		task->cancel(caller_location);
	}

	void reset_deadline(){
		if(task->supervisor){
			task->supervisor->reset_deadline(task);
		}
	}

	u32 id() const {
		return task->id;
	}
};

constexpr auto _task_nop = [](TaskContext) -> Unit { return{}; };

constexpr inline auto _cancellation_nop = [](TaskContext){ /* Nothing */ };

//// Integrity checking

u32 crc32_advance(u32 current, Slice<u8> buf);

template<typename T>
struct CRC32 {
	u32 get(T const& data) = delete;
};

template<>
struct CRC32<Slice<u8>> {
	u32 get(Slice<u8> data);
};

template<typename T>
concept CRC32_Checkable = requires(T const& obj) {
	{ CRC32<T>{}.get(obj) } -> SameAs<u32>;
};

template<CRC32_Checkable T>
attribute_force_inline
u32 crc32(T const& obj){
	return CRC32<T>{}.get(obj);
}

template<typename T>
	requires CRC32_Checkable<T>
void crc32_ensure(u32 expected, T const& obj, CALLER_LOCATION){
    volatile u32 cur = CRC32<T>{}.get(obj);
    if(expected != cur){
        panic("Failed CRC32 check", caller_location);
    }
}

template<typename T>
	requires CRC32_Checkable<T>
void crc32_ensure(u32 expected, CRC32_Checkable auto const& obj, TaskContext* ctx, CALLER_LOCATION){
    volatile u32 cur = CRC32<T>{}.get(obj);
    if(expected != cur){
        ctx->panic("Failed CRC32 check", caller_location);
    }
}

//// Basic task
template<typename Output, Callable<Output, TaskContext> TaskFunc, Callable<void, TaskContext> OnCancel>
struct BasicTask {
	RawTask _task;
	TaskFunc _func;
	OnCancel _on_cancel;
	Option<Output> _result;

	static void _basic_task_wrapper(RawTask* t){
		auto self = (BasicTask<Output, TaskFunc, OnCancel>*)t->args;
		auto context = TaskContext { &self->_task };
		self->_result = Output{ self->_func(context) };
	}

	static void _basic_task_cancel_wrapper(RawTask* t){
		auto self = (BasicTask<Output, TaskFunc, OnCancel>*)t->args;
		auto context = TaskContext { &self->_task };
		self->_on_cancel(context);
	}

	Option<Output> result(){
		if(_task._status.load(memory_order_relaxed) == TaskStatus_Done){
			return move(_result);
		}
		return {}	;
	}

	bool has_result() const {
		return _result.ok() && _task._status.load(memory_order_relaxed) == TaskStatus_Done;
	}

	attribute_force_inline
	TaskStatus status() const {
		return _task._status.load(memory_order_relaxed);
	}

	void join(){
		_task.join();
	}

	RawTask* raw_task() {
		return &_task;
	}

	u32 id(){
		return _task.id;
	}

	[[nodiscard]]
	bool attach_supervisor(DeadlineWatcher* watcher, Duration limit){
		return _task.attach_supervisor(watcher, limit);
	}

	void cancel(CALLER_LOCATION){
		_task.cancel(caller_location);

		// NOTE: Mainly for safety, ensure that whatever was here is zeroed,
		// this can lead to edge case leaks when the result is partially
		// initialized
		mem_zero(&_result, sizeof(_result));
	}

	explicit BasicTask(TaskFunc f)
		: _task{}
		, _func{f}
		, _on_cancel{_cancellation_nop} {}

	explicit BasicTask(TaskFunc f, OnCancel c)
		: _task{}
		, _func{f}
		, _on_cancel{c}
	{}
};

static_assert(Task<BasicTask<Unit, decltype(_task_nop), decltype(_cancellation_nop)>, Unit>, "BasicTask does not conform to Task concept");

template<typename F>
auto make_basic_task(Arena* parent, usize task_arena_size, usize stack_size, F&& func){
	using TaskType = BasicTask<decltype(func(TaskContext{})), F, decltype(_cancellation_nop)>;
	auto t = make<TaskType>(parent, forward<F>(func));
	if(!t){
		return (TaskType*)nullptr;
	}
	t->_task.on_cancel = t->_basic_task_cancel_wrapper;

	if(!init_raw_task(&t->_task, parent, task_arena_size, stack_size, t->_basic_task_wrapper, t)){
		return (TaskType*)nullptr;
	}

	return t;
}

template<typename F>
auto make_basic_task(Arena* parent, usize task_arena_size, F&& func){
	return make_basic_task(parent, task_arena_size, 0, forward<F>(func));
}

//// Software watchdog timer
void swdg_init(Duration n);

void swdg_reset();

void swdg_stop();

void swdg_resume();

