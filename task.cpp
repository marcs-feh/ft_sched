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

