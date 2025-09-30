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
	virtual void fault() = 0;

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

#if defined(__linux__)
	struct RawTaskPlatformSpecificData {
		uintptr data[2];
	};
#else
	#error "Specify target platform"
#endif

struct RawTask : Task {
	RawTaskFunc func;
	Arena* arena;
	void* args;
	void* result;
	Atomic<TaskStatus> _status = TaskStatus_Undefined;
	RawTaskPlatformSpecificData _specific{};

	void run() override {
		_init_platform_specific();
	}

	TaskStatus status() override {
		return _status;
	}

	void fault() override {
		_status.store(TaskStatus_Fault);
	}
	
	void drop(){
		_finish_platform_specific();
	}

	~RawTask(){
		drop();
	}

	void _init_platform_specific();
	void _finish_platform_specific();
};

RawTask* make_raw_task(Arena* a, RawTaskFunc func, void* args);

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

