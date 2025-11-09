#include "ft_sched.hpp"

static Atomic<u32> raw_task_id_counter = 1;

u32 next_raw_task_id(){
	return raw_task_id_counter.fetch_add(1, memory_order_relaxed);
}

bool init_raw_task(RawTask* task, Arena* parent, usize sub_arena_size, usize stack_size, RawTaskFunc func, void* args){
	ensure(task != nullptr, "Must be non-null");
	auto restore = parent->offset;

	// Enough to hold the raw task itself + minimum stack + some padding
	stack_size = max(task_min_stack_size, stack_size);
	auto min_sub_arena_size = stack_size + sizeof(RawTask) + (sizeof(void*) * 2);

	task->arena = parent->make_sub(max<usize>(sub_arena_size, min_sub_arena_size));
	if(!task->arena){
		return false;
	}

	task->func = func;
	task->args = args;
	task->id = next_raw_task_id();
	task->stack_size = stack_size;

	task->_status.store(TaskStatus_Initialized);

	auto ok = task->_platform_init(task->arena, stack_size, func, args);
	if(!ok){
		// Not enough space, roll back arena
		parent->offset = restore;
	}
	return ok;
}

RawTask* make_raw_task(Arena* parent, usize sub_arena_size, usize stack_size, RawTaskFunc func, void* args){
	auto restore = parent->offset;
	auto task = make<RawTask>(parent);

	if(task && init_raw_task(task, parent, sub_arena_size, stack_size, func, args)){
		return task;
	}

	parent->offset = restore;
	return nullptr;
}

static inline
void _raw_task_slot_cancellation(void* data){
	auto t = (RawTask*)data;
	printf("SLOT CANCEL:");
	t->cancel();
}

[[nodiscard]]
bool RawTask::attach_supervisor(DeadlineWatcher* watcher, Duration limit){
	auto ok = watcher->add((void*)this, _raw_task_slot_cancellation, limit);
	if(ok){
		ensure(this->supervisor == nullptr || this->supervisor == watcher, "Task already has a supervisor attached");
		this->supervisor = watcher;
	}
	return ok;
}

// bool init_tmr_task(TMR_Task* task, Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args){
// 	auto restore = a->offset;

// 	auto arena0 = a->make_sub(subtask_arena_size);
// 	auto arena1 = a->make_sub(subtask_arena_size);
// 	auto arena2 = a->make_sub(subtask_arena_size);

// 	auto arenas_ok = arena0 && arena1 && arena2;
// 	if(!arenas_ok){
// 		a->offset = restore;
// 		return false;
// 	}

// 	init_raw_task(&task->task0, arena0, func, args);
// 	init_raw_task(&task->task1, arena1, func, args);
// 	init_raw_task(&task->task2, arena2, func, args);

// 	return true;
// }

// TMR_Task* make_tmr_task(Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args){
// 	auto task = make<TMR_Task>(a);
// 	if(task == nullptr) { return nullptr; }

// 	if(!init_tmr_task(task, a, subtask_arena_size, func, args)){
// 		return nullptr;
// 	}
// 	return task;
// }
