#define WIN32_LEAN_AND_MEAN
#include "task.hpp"
#include <windows.h>
#include <stdio.h>

struct RawTaskPlatformSpecific {
	HANDLE handle;
	DWORD id;
};

static
DWORD task_windows_wrapper(LPVOID arg){
	auto task = (RawTask*)arg;
	task->_status.store(TaskStatus_Started);

	task->func(task);

	task->_status.store(TaskStatus_Done);
	return 0;
}
	
void RawTask::_init_specifics_and_run(){
	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	ensure(_status.load() == TaskStatus_Initialized, "Invalid task status");

	DWORD id;
	HANDLE handle = CreateThread(NULL, 0, task_windows_wrapper, (LPVOID)this, 0, &id);
	ensure(handle != NULL, "Failed to create thread");
	specific->handle = handle;
}

void RawTask::_join_and_deinit_specifics(){
	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	DWORD ret = WaitForSingleObject(specific->handle, INFINITE);
	ensure(ret != WAIT_FAILED, "failed to join");

	auto status = this->_status.load();
	ensure(status == TaskStatus_Fault || status == TaskStatus_Done, "Invalid task status");
	CloseHandle(specific->handle);
}

static_assert(sizeof(RawTaskPlatformSpecific) <= sizeof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient size");
static_assert(alignof(RawTaskPlatformSpecific) <= alignof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient size");
