#pragma once
#include "base.hpp"

enum TaskStatus : u8 {
	TaskStatus_Undefined = 0,
	TaskStatus_Initialized = 1,
	TaskStatus_Started = 2,
	TaskStatus_Done = 3,

	TaskStatus_Fault, // Or anthing above
};

struct Task {
	virtual void run() = 0;
	virtual TaskStatus status() = 0;
	virtual void join() = 0;
	virtual void fault() = 0;
	// virtual Arena* local_arena() = 0;

	virtual ~Task(){}
};

template<typename F>
concept TaskBody = requires(F f, Task* e){
	{ f(e) } -> SameAs<void>;
};

struct RawTask;

// Task object that closely maps to a regular thread
using RawTaskFunc = void (*)(RawTask* t);

struct RawTaskPlatformSpecificData {
	uintptr data[2];
};

// TODO(marcos): Just make this enforced
constexpr usize default_argument_alignment = alignof(void*) * 2;

struct RawTask : Task {
	RawTaskFunc func = nullptr;
	Arena* arena = nullptr;
	void* args = nullptr;
	u32 args_size;
	Atomic<TaskStatus> _status = TaskStatus_Initialized;
	void* result = nullptr;
	usize result_size = 0;

	RawTaskPlatformSpecificData _specific{};

	void run() override {
		_init_specifics_and_run();
	}

	TaskStatus status() override {
		return _status;
	}

	void fault() override {
		_status.store(TaskStatus_Fault);
	}

	void join() override {
		_join_and_deinit_specifics();
	}

	~RawTask(){}

	void _init_specifics_and_run();
	void _join_and_deinit_specifics();
};

void init_raw_task(RawTask* task, Arena* a, RawTaskFunc func, void* args);

RawTask* make_raw_task(Arena* a, RawTaskFunc func, void* args);

struct TMR_Task : Task {
	Arena* arena = nullptr;
	RawTaskFunc func = nullptr;
	void* args = nullptr;
	u32 args_size = 0;

	RawTask task0{};
	RawTask task1{};
	RawTask task2{};

	void run() override {
		auto init = (task0.status() == TaskStatus_Initialized)
			&& (task1.status() == TaskStatus_Initialized)
			&& (task2.status() == TaskStatus_Initialized);

		ensure(init, "Sub-tasks are not properly initialized");

		this->task0.run();
		this->task1.run();
		this->task2.run();
	}

	TaskStatus status() override {
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

	void fault() override {
		task0.fault();
		task1.fault();
		task2.fault();
	}

	// TODO: Use a timeout
	void join() override {
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

bool init_tmr_task(RawTask* task, Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args, usize args_size, usize args_align = default_argument_alignment);

TMR_Task* make_tmr_task(Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args, usize args_size, usize args_align = default_argument_alignment);

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

	Output result(){
		if(_task._status.load() != TaskStatus_Done){
			_task.join();
		}
		return _result.unwrap();
	}

	bool has_result() const {
		return _result.ok() && _task._status.load(memory_order_relaxed) == TaskStatus_Done;
	}

	attribute_force_inline
	TaskStatus status() const {
		return _task._status.load(memory_order_relaxed);
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
