#define _DEFAULT_SOURCE
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

void RawTask::_init_specifics_and_run(){
	ensure(_status.load() == TaskStatus_Initialized, "Invalid task status");

	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	auto ok = pthread_create(&specific->handle, nullptr, task_linux_wrapper, (void*)this) == 0;

	ensure(ok, "Failed to create thread");
}

void RawTask::_join_and_deinit_specifics(){
	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	pthread_join(specific->handle, NULL);

	auto status = this->_status.load();
	ensure(status == TaskStatus_Fault || status == TaskStatus_Done, "Invalid task status");
}

static_assert(sizeof(RawTaskPlatformSpecific) <= sizeof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient size");
static_assert(alignof(RawTaskPlatformSpecific) <= alignof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient size");
