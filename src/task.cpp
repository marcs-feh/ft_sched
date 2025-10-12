#include "task.hpp"

bool init_raw_task(RawTask* task, Arena* a, RawTaskFunc func, void* args, usize arg_size, usize arg_align){
	ensure(task != nullptr, "Must be non-null");

	auto restore = a->offset;

	task->func = func;
	task->arena = a;
	task->_status.store(TaskStatus_Initialized);

	if(args != nullptr){
		task->args = a->alloc(arg_size, arg_align);
		if(task->args != nullptr){
			mem_copy(task->args, args, arg_size);
		}
		else {
			a->offset = restore;
			return false;
		}
	}

	return true;
}

RawTask* make_raw_task(Arena* a, RawTaskFunc func, void* args, usize arg_size, usize arg_align){
	auto restore = a->offset;
	auto task = make<RawTask>(a);

	if(!init_raw_task(task, a, func, args, arg_size, arg_align)){
		a->offset = restore;
		return nullptr;
	}

	return task;
}

bool init_tmr_task(TMR_Task* task, Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args, usize args_size, usize args_align){
	panic("Unimplemented");
}

TMR_Task* make_tmr_task(Arena* a, u32 subtask_arena_size, RawTaskFunc func, void* args, usize args_size, usize args_align){
	auto task = make<TMR_Task>(a);
	if(task == nullptr) { return nullptr; }

	if(!init_tmr_task(task, a, subtask_arena_size, func, args, args_size, args_align)){
		return nullptr;
	}
	return task;
}
