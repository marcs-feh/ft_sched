#define _DEFAULT_SOURCE
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#include "ft_sched.hpp"
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

TimeTick tick_now(){
	struct timespec tspec = {};
	if(clock_gettime(CLOCK_MONOTONIC_RAW, &tspec) < 0){
		panic("Failed to get clock");
	}
	return (tspec.tv_sec * 1'000'000'000) + tspec.tv_nsec;
}

usize _tick_frequency = 0;

usize tick_frequency(){
	if(!_tick_frequency){
		struct timespec tspec = {};
		if(clock_getres(CLOCK_MONOTONIC_RAW, &tspec) < 0){
			panic("Failed to get resolution");
		}
		ensure(tspec.tv_sec == 0, "Resolution is too slow, wtf is wrong with your clock?");

		_tick_frequency = 1'000'000'000 / tspec.tv_nsec;
	}

	return _tick_frequency;
}

void sleep_for(Duration d){
	usize nanosecs = d.to_micro() * 1'000;
	usize secs = nanosecs / 1'000'000'000;
	nanosecs -= secs * 1'000'000'000;

	struct timespec tspec = {};
	tspec.tv_sec = secs;
	tspec.tv_nsec = nanosecs;

	while(1){
		if(nanosleep(&tspec, &tspec) != EINTR){
			break;
		}
	}
}

static_assert(sizeof(RawTaskPlatformSpecific) <= sizeof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient size");
static_assert(alignof(RawTaskPlatformSpecific) <= alignof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient size");
