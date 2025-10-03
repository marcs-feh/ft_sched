#include "task.hpp"

RawTask* make_raw_task(Arena* a, RawTaskFunc func, void* args, usize arg_size, usize arg_align){
	auto restore = a->offset;
	auto raw_task = make<RawTask>(a);
	bool ok = true;

	if(raw_task != nullptr){
		raw_task->func = func;
		raw_task->arena = a;
		raw_task->_status.store(TaskStatus_Initialized);

		if(args != nullptr){
			raw_task->args = a->alloc(arg_size, arg_align);
			if(raw_task->args != nullptr){
				mem_copy(raw_task->args, args, arg_size);
			}
			else {
				ok = false;
			}
		}
	}
	else {
		ok = false;
	}

	if(!ok){
		a->offset = restore;
		return nullptr;
	}

	return raw_task;
}

