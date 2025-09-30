#define _DEFAULT_SOURCE/
#include <pthread.h>
#include <semaphore.h>

#include "task.hpp"

struct RawTaskPlatformSpecific {
	pthread_t handle;
};

static
void* task_linux_wrapper(void* task_ptr){
	RawTask* task = (RawTask*)task_ptr;
	task->_status.store(TaskStatus_Started);

	task->func(task);

	task->_status.store(TaskStatus_Done);
	return NULL;
}

void RawTask::_init_platform_specific(){
	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	auto ok = pthread_create(&specific->handle, nullptr, task_linux_wrapper, (void*)this) == 0;
	ensure(ok, "Failed to create thread");
}

void RawTask::_finish_platform_specific(){
	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	pthread_join(specific->handle, NULL);

	auto status = this->_status.load();
	ensure(status == TaskStatus_Fault || status == TaskStatus_Done, "Invalid task status");
}

// TODO: Maybe copy args into arena
RawTask* make_raw_task(Arena* a, RawTaskFunc func, void* args){
	auto raw_task = make<RawTask>(a);
	if(raw_task){
		raw_task->func = func;
		raw_task->arena = a;
		raw_task->args = args;
	}
	return raw_task;
}

static_assert(sizeof(RawTaskPlatformSpecific) <= sizeof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient size");
static_assert(alignof(RawTaskPlatformSpecific) <= alignof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient size");
