#include "ft_sched.hpp"

static Atomic<u32> raw_task_id_counter = 1;

u32 next_raw_task_id(){
	return raw_task_id_counter.fetch_add(1, memory_order_relaxed);
}

// TODO: use a sub-arena to avoid ownership issues
void init_raw_task(RawTask* task, Arena* a, usize stack_size, RawTaskFunc func, void* args){
	ensure(task != nullptr, "Must be non-null");

	task->func = func;
	task->arena = a;
	task->stack_size = stack_size;
	task->_status.store(TaskStatus_Initialized);
	task->args = args;
	task->id = next_raw_task_id();
}

void init_raw_task(RawTask* task, Arena* a, RawTaskFunc func, void* args){
	init_raw_task(task, a, 0, func, args);
}

RawTask* make_raw_task(Arena* a, RawTaskFunc func, void* args){
	auto task = make<RawTask>(a);

	if(task){
		init_raw_task(task, a, func, args);
	}

	return task;
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
