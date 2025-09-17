#include "base.hpp"

enum TaskStatus : u8 {
	TaskStatus_Undefined = 0,
	TaskStatus_Initialized = 1,
	TaskStatus_Started = 2,
	TaskStatus_Done = 3,

	TaskStatus_Fault, // Or anthing above
};

struct Executable {
	virtual void run() = 0;
	virtual TaskStatus status() = 0;
	virtual void fault() = 0;

	virtual ~Executable(){}
};

template<typename F>
concept TaskBody = requires(F f, Executable* e){
	{ f(e) } -> SameAs<void>;
};

struct Deadline {
	usize limit;
	usize start;
};

void _task_run(void*);
	
template<TaskBody Fn>
struct Task : Executable {
	Atomic<TaskStatus> _status = TaskStatus_Undefined;
	Fn body;
	usize stack_size;
	u8* stack_data;

	void run() override {
		_status.store(TaskStatus_Started);

		// _task_run(this);
		body(this);

		TaskStatus expected = TaskStatus_Started;
		if(!_status.compare_exchange_strong(expected, TaskStatus_Done, memory_order_seq_cst, memory_order_relaxed)){
			_status.store(TaskStatus_Fault);
		}
	}

	TaskStatus status() override {
		return _status;
	}

	void fault() override {
		_status.store(TaskStatus_Fault);
	}

	Task() = delete;

	explicit Task(Fn body, Slice<u8> stack)
		: body{body}
		, stack_size{stack.len}
		, stack_data{stack_data}
		, _status{TaskStatus_Undefined}
	{
		_status.store(TaskStatus_Initialized);
	}

	~Task(){}
};

template<TaskBody Fn>
Task<Fn>* make_task(Arena* arena, Fn&& body, usize stack_size){
	auto restore_offset = arena->offset;

	auto t = make_unitialized<Task<Fn>>(arena, body);
	auto stack = make_slice<u8>(arena, stack_size);

	if(t == nullptr || stack.len == 0){
		arena->offset = restore_offset;
		return nullptr;
	}

	new (&t) Task<Fn>(forward<Fn>(body), stack);

	return t;
}

