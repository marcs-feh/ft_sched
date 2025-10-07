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

struct Deadline {
	usize limit;
	usize start;
};

struct RawTask;

// Task object that closely maps to a regular thread
using RawTaskFunc = void (*)(RawTask* t);

#if defined(TARGET_HOSTED_LINUX) || defined(TARGET_HOSTED_WINDOWS)
	struct RawTaskPlatformSpecificData {
		uintptr data[2];
	};
#else
	#error "Specify target platform"
#endif

// TODO(marcos): Just make this enforced
constexpr usize default_argument_alignment = alignof(void*) * 2;

struct RawTask : Task {
	RawTaskFunc func = nullptr;
	Arena* arena = nullptr;
	void* args = nullptr;
	u32 args_size;
	Atomic<TaskStatus> _status = TaskStatus_Initialized;
	void* result = nullptr;

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

bool init_raw_task(RawTask* task, Arena* a, RawTaskFunc func, void* args, usize args_size, usize args_align = default_argument_alignment);

RawTask* make_raw_task(Arena* a, RawTaskFunc func, void* args, usize args_size, usize args_align = default_argument_alignment);

struct TMR_Task : Task {
	Arena* arena = nullptr;
	RawTaskFunc func = nullptr;
	void* args = nullptr;
	u32 args_size = 0;
	u32 subtask_arena_size = 0;

	RawTask task0{};
	RawTask task1{};
	RawTask task2{};

	void run() override {
		auto arena0 = this->arena->make_sub(subtask_arena_size);
		auto arena1 = this->arena->make_sub(subtask_arena_size);
		auto arena2 = this->arena->make_sub(subtask_arena_size);

		ensure((arena0 != nullptr) && (arena1 != nullptr) && (arena2 != nullptr), "Failed to allocate sub-task arenas");

		bool ok =
			init_raw_task(&this->task0, arena0, func, args, args_size, default_argument_alignment) &&
			init_raw_task(&this->task1, arena1, func, args, args_size, default_argument_alignment) &&
			init_raw_task(&this->task2, arena2, func, args, args_size, default_argument_alignment);

		ensure(ok, "Failed to init sub-tasks");

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
		panic("Unimplemented");
	}

	// TODO: Use a timeout
	void join() override {
		task0->join();
		task1->join();
		task2->join();
	}
};

bool init_tmr_task(RawTask* task, Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args, usize args_size, usize args_align = default_argument_alignment);

TMR_Task* make_tmr_task(Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args, usize args_size, usize args_align = default_argument_alignment);

/// Task object that is a thin wrapper over a regular function
// template<TaskBody Fn>
// struct FnTask : Task {
// 	Atomic<TaskStatus> _status = TaskStatus_Undefined;
// 	Fn body;
// 	usize stack_size;
// 	u8* stack_data;

// 	void* _platform_handle = nullptr;

// 	void run() override {
// 		_status.store(TaskStatus_Started);

// 		body(this);

// 		TaskStatus expected = TaskStatus_Started;
// 		if(!_status.compare_exchange_strong(expected, TaskStatus_Done, memory_order_seq_cst, memory_order_relaxed)){
// 			_status.store(TaskStatus_Fault);
// 		}
// 	}

// 	TaskStatus status() override {
// 		return _status;
// 	}

// 	void fault() override {
// 		_status.store(TaskStatus_Fault);
// 	}

// 	FnTask() = delete;

// 	explicit FnTask(Fn body, Slice<u8> stack)
// 		: body{body}
// 		, stack_size{stack.len}
// 		, stack_data{stack_data}
// 		, _status{TaskStatus_Undefined}
// 	{
// 		_status.store(TaskStatus_Initialized);
// 		task_close_platform_handle(_platform_handle);
// 	}

// 	~FnTask(){}
// };

// template<TaskBody Fn>
// FnTask<Fn>* make_task(Arena* arena, Fn&& body, usize stack_size){
// 	auto restore_offset = arena->offset;

// 	auto t = make_unitialized<FnTask<Fn>>(arena, body);
// 	auto stack = make_slice<u8>(arena, stack_size);

// 	if(t == nullptr || stack.len == 0){
// 		arena->offset = restore_offset;
// 		return nullptr;
// 	}

// 	new (&t) FnTask<Fn>(forward<Fn>(body), stack);

// 	return t;
// }

