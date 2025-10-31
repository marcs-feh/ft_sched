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
using TimeTick = isize;

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

//// Integrity checking
template<typename T>
concept CRC32_Checkable = requires(T const& obj) {
	{ crc32(obj) } -> SameAs<u32>;
};

u32 crc32(Slice<u8> buf);

void crc32_ensure(u32 expected, CRC32_Checkable auto const& obj){
    volatile u32 cur = crc32(obj);
    if(expected != cur){
        panic("Failed CRC32 check");
    }
}

//// Tasks
enum TaskStatus : u8 {
	TaskStatus_Undefined = 0,
	TaskStatus_Initialized = 1,
	TaskStatus_Started = 2,
	TaskStatus_Done = 3,

	TaskStatus_Fault, // Or anthing above
};

template<typename Impl, typename T>
concept Task = requires(Impl impl){
	{ impl.run() } -> SameAs<void>;
	{ impl.result() } -> SameAs<Option<T>>;
	{ impl.status() } -> SameAs<TaskStatus>;
	{ impl.cancel() } -> SameAs<void>;
	{ impl.join() } -> SameAs<void>;
	// { impl.fault() } -> SameAs<void>;
};

struct RawTask;

// Task object that closely maps to a regular thread
using RawTaskFunc = void (*)(RawTask* t);

struct RawTaskPlatformSpecificData {
	uintptr data[2];
};

using TaskCancelCallback = void (*)(RawTask* self);

struct RawTask {
	RawTaskFunc func = nullptr;
	Arena* arena = nullptr;
	void* args = nullptr;
	u32 args_size;
	Atomic<TaskStatus> _status = TaskStatus_Initialized;

	// Optional callback to unlock mutexes or release any additional resource after killing the thread
	TaskCancelCallback on_cancel = nullptr;

	RawTaskPlatformSpecificData _specific{};

	void run() {
		_init_specifics_and_run();
	}

	TaskStatus status() {
		return _status;
	}

	void fault() {
		_status.store(TaskStatus_Fault);
	}

	void join() {
		_join_and_deinit_specifics();
	}

	void cancel(){
		_cancel_and_deinit_specifics();
		if(on_cancel)
			on_cancel(this);
	}

	~RawTask(){}

	void _init_specifics_and_run();
	void _join_and_deinit_specifics();
	void _cancel_and_deinit_specifics();
};

void init_raw_task(RawTask* task, Arena* a, RawTaskFunc func, void* args);

RawTask* make_raw_task(Arena* a, RawTaskFunc func, void* args);

template<typename F, typename Output, typename ... Args>
concept Callable = requires(F f, Args... args){
	{ f(args...) } -> SameAs<Output>;
};

extern "C" int printf(cstring fmt, ...);
constexpr inline auto _cancellation_nop = [](){ printf("BYEEEE\n");/* Nothing */ };

template<typename Output, Callable<Output> TaskFunc, Callable<void> OnCancel>
struct BasicTask {
	RawTask _task;
	TaskFunc _func;
	OnCancel _on_cancel;
	Option<Output> _result;

	static void _basic_task_wrapper(RawTask* t){
		auto self = (BasicTask<Output, TaskFunc, OnCancel>*)t->args;
		self->_result = Output{ self->_func() };
	}

	static void _basic_task_cancel_wrapper(RawTask* t){
		auto self = (BasicTask<Output, TaskFunc, OnCancel>*)t->args;
		self->_on_cancel();
	}

	void run(){
		ensure(_task._status.load(memory_order_relaxed) == TaskStatus_Initialized, "Inner task has not been initialized");
		_task.run();
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

	void cancel(){
		_task.cancel();

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

template<typename F>
auto make_basic_task(Arena* a, F&& func){
	auto t = make<BasicTask<decltype(func()), F, decltype(_cancellation_nop)>>(a, forward<F>(func));

	init_raw_task(&t->_task, a, t->_basic_task_wrapper, t);
	t->_task.on_cancel = t->_basic_task_cancel_wrapper;

	return t;
}

template<typename F, Callable<void> C>
auto make_basic_task(Arena* a, F&& func, C&& cancel){
	auto t = make<BasicTask<decltype(func()), F, C>>(a, forward<F>(func), forward<C>(cancel));

	init_raw_task(&t->_task, a, t->_basic_task_wrapper, t);
	t->_task.on_cancel = t->_basic_task_cancel_wrapper;
}

// struct TMR_Task {
// 	Arena* arena = nullptr;
// 	RawTaskFunc func = nullptr;
// 	void* args = nullptr;
// 	u32 args_size = 0;

// 	RawTask task0{};
// 	RawTask task1{};
// 	RawTask task2{};

// 	void run() {
// 		auto init = (task0.status() == TaskStatus_Initialized)
// 			&& (task1.status() == TaskStatus_Initialized)
// 			&& (task2.status() == TaskStatus_Initialized);

// 		ensure(init, "Sub-tasks are not properly initialized");

// 		this->task0.run();
// 		this->task1.run();
// 		this->task2.run();
// 	}

// 	TaskStatus status() {
// 		auto status0 = task0.status();
// 		auto status1 = task1.status();
// 		auto status2 = task2.status();

// 		auto all_done =
// 			(status0 == TaskStatus_Done) &&
// 			(status1 == TaskStatus_Done) &&
// 			(status2 == TaskStatus_Done);

// 		if(all_done){
// 			return TaskStatus_Done;
// 		}

// 		auto faulted =
// 			(status0 == TaskStatus_Fault) ||
// 			(status1 == TaskStatus_Fault) ||
// 			(status2 == TaskStatus_Fault);

// 		if(faulted){
// 			return TaskStatus_Fault;
// 		}

// 		auto at_least_started =
// 			(status0 >= TaskStatus_Started) &&
// 			(status1 >= TaskStatus_Started) &&
// 			(status2 >= TaskStatus_Started);

// 		if(at_least_started){
// 			return TaskStatus_Started;
// 		}

// 		auto at_least_initialized =
// 			(status0 >= TaskStatus_Initialized) &&
// 			(status1 >= TaskStatus_Initialized) &&
// 			(status2 >= TaskStatus_Initialized);

// 		if(at_least_initialized){
// 			return TaskStatus_Initialized;
// 		}

// 		return TaskStatus_Undefined;
// 	}

// 	void fault() {
// 		task0.fault();
// 		task1.fault();
// 		task2.fault();
// 	}

// 	// TODO: Use a timeout
// 	void join() {
// 		if(task0.status() != TaskStatus_Fault){
// 			task0.join();
// 		}

// 		if(task1.status() != TaskStatus_Fault){
// 			task1.join();
// 		}

// 		if(task2.status() != TaskStatus_Fault){
// 			task2.join();
// 		}
// 	}
// };

// bool init_tmr_task(RawTask* task, Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args, usize args_size);

// TMR_Task* make_tmr_task(Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args, usize args_size);

//// Deadlines
struct DeadlineSlot {
	int value;

	TimeTick last_tick;
	Duration limit;
	RawTask* task;

	void reset(){
		last_tick = tick_now();
	}
};

struct DeadlineWatcher {
	Slice<DeadlineSlot> slots;
	Spinlock _lock{};

	auto lock_guard(){
		return _lock.guard();
	}

	[[nodiscard]]
	DeadlineSlot* add(RawTask* task, Duration limit);

	void _remove_no_lock(DeadlineSlot* node);

	void remove(DeadlineSlot* node);

	void clear();

	// Scan for deadline violations and remove Done tasks
	bool scan();

	DeadlineWatcher()
		: slots{}
		, _lock{}
	{}

	// void display(){
	// 	printf("Free: ");
	// 	for(auto const& slot : slots){
	// 		if(!slot.task){
	// 			printf(" . ");
	// 		}
	// 	}
	// 	printf("\n");

	// 	printf("Check: ");
	// 	for(auto const& slot : slots){
	// 		if(slot.task){
	// 			printf("%d ", (int)slot.limit.to_milli());
	// 		}
	// 	}
	// 	printf("\n");
	// }
};

void init_deadline_watcher(DeadlineWatcher* w, Slice<DeadlineSlot> slots);

DeadlineWatcher* make_deadline_watcher(Arena* a, usize slot_count);


#if 0
struct TMR_Task {
	Arena* arena = nullptr;
	RawTaskFunc func = nullptr;
	void* args = nullptr;
	u32 args_size = 0;

	RawTask task0{};
	RawTask task1{};
	RawTask task2{};

	void run() {
		auto init = (task0.status() == TaskStatus_Initialized)
			&& (task1.status() == TaskStatus_Initialized)
			&& (task2.status() == TaskStatus_Initialized);

		ensure(init, "Sub-tasks are not properly initialized");

		this->task0.run();
		this->task1.run();
		this->task2.run();
	}

	TaskStatus status() {
		auto status0 = task0.status();
		auto status1 = task1.status();
		auto status2 = task2.status();

		auto all_done =
			(status0 == TaskStatus_Done) &&
			(status1 == TaskStatus_Done) &&
			(status2 == TaskStatus_Done);

		if(all_done){
			return TaskStatus_Done;
		}

		auto faulted =
			(status0 == TaskStatus_Fault) ||
			(status1 == TaskStatus_Fault) ||
			(status2 == TaskStatus_Fault);

		if(faulted){
			return TaskStatus_Fault;
		}

		auto at_least_started =
			(status0 >= TaskStatus_Started) &&
			(status1 >= TaskStatus_Started) &&
			(status2 >= TaskStatus_Started);

		if(at_least_started){
			return TaskStatus_Started;
		}

		auto at_least_initialized =
			(status0 >= TaskStatus_Initialized) &&
			(status1 >= TaskStatus_Initialized) &&
			(status2 >= TaskStatus_Initialized);

		if(at_least_initialized){
			return TaskStatus_Initialized;
		}

		return TaskStatus_Undefined;
	}

	void fault() {
		task0.fault();
		task1.fault();
		task2.fault();
	}

	// TODO: Use a timeout
	void join() {
		if(task0.status() != TaskStatus_Fault){
			task0.join();
		}

		if(task1.status() != TaskStatus_Fault){
			task1.join();
		}

		if(task2.status() != TaskStatus_Fault){
			task2.join();
		}
	}
};

bool init_tmr_task(RawTask* task, Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args, usize args_size);

TMR_Task* make_tmr_task(Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args, usize args_size);
#endif
