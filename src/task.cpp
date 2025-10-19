#include "task.hpp"

bool init_raw_task(RawTask* task, Arena* a, RawTaskFunc func, void* args, usize args_, usize args_align){
	ensure(task != nullptr, "Must be non-null");

	auto restore = a->offset;

	task->func = func;
	task->arena = a;
	task->_status.store(TaskStatus_Initialized);

	if(args != nullptr){
		task->args = a->alloc(args_, args_align);
		if(task->args != nullptr){
			mem_copy(task->args, args, args_);
		}
		else {
			a->offset = restore;
			return false;
		}
	}

	return true;
}

RawTask* make_raw_task(Arena* a, RawTaskFunc func, void* args, usize args_size, usize args_align){
	auto restore = a->offset;
	auto task = make<RawTask>(a);

	if(!init_raw_task(task, a, func, args, args_size, args_align)){
		a->offset = restore;
		return nullptr;
	}

	return task;
}

bool init_tmr_task(TMR_Task* task, Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args, usize args_size, usize args_align){
	auto restore = a->offset;

	auto arena0 = a->make_sub(subtask_arena_size);
	auto arena1 = a->make_sub(subtask_arena_size);
	auto arena2 = a->make_sub(subtask_arena_size);

	auto arenas_ok = arena0 && arena1 && arena2;
	if(!arenas_ok){
		a->offset = restore;
		return false;
	}

	auto init_ok = init_raw_task(&task->task0, arena0, func, args, args_size, args_align)
		&& init_raw_task(&task->task1, arena1, func, args, args_size, args_align)
		&& init_raw_task(&task->task2, arena2, func, args, args_size, args_align);

	if(!init_ok){
		a->offset = restore;
		return false;
	}

	return true;
}

TMR_Task* make_tmr_task(Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args, usize args_size, usize args_align){
	auto task = make<TMR_Task>(a);
	if(task == nullptr) { return nullptr; }

	if(!init_tmr_task(task, a, subtask_arena_size, func, args, args_size, args_align)){
		return nullptr;
	}
	return task;
}
