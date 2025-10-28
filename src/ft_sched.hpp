#pragma once
#include "base.hpp"
//// Compiler intrinsics

#if defined(__clang__) || defined(__GNUC__)
	// Cause a full memory clobber, this emits no CPU instructions but prevents the compiler from doing certain loads
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

void crc32_ensure(volatile u32 expected, CRC32_Checkable auto const& obj){
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
	{ impl.join() } -> SameAs<void>;
	// { impl.fault() } -> SameAs<void>;
};

struct RawTask;

// Task object that closely maps to a regular thread
using RawTaskFunc = void (*)(RawTask* t);

struct RawTaskPlatformSpecificData {
	uintptr data[2];
};

struct RawTask {
	RawTaskFunc func = nullptr;
	Arena* arena = nullptr;
	void* args = nullptr;
	u32 args_size;
	Atomic<TaskStatus> _status = TaskStatus_Initialized;

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

	~RawTask(){}

	void _init_specifics_and_run();
	void _join_and_deinit_specifics();
};

void init_raw_task(RawTask* task, Arena* a, RawTaskFunc func, void* args);

RawTask* make_raw_task(Arena* a, RawTaskFunc func, void* args);


template<typename F, typename Output>
concept Returns = requires(F f){
	{ f() } -> SameAs<Output>;
};

template<typename Output, Returns<Output> TaskFunc>
struct BasicTask {
	RawTask _task;
	TaskFunc _func;
	Option<Output> _result;

	static void _simple_task_wrapper(RawTask* t){
		auto self = (BasicTask<Output, TaskFunc>*)t->args;
		self->_result = Output{ self->_func() };
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

	explicit BasicTask(TaskFunc f)
		: _task{}
		, _func{f} {}
};

template<typename F>
auto make_basic_task(Arena* a, F&& func){
	auto t = make<BasicTask<decltype(func()), F>>(a, forward<F>(func));

	init_raw_task(&t->_task, a, t->_simple_task_wrapper, t);
	return t;
}


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