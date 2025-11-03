#define _DEFAULT_SOURCE
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

//// Base platform specifics
#include "base.hpp"

extern "C" {
	#include <sys/types.h>
	void abort();
	ssize_t write(int fd, void const* buf, size_t count);
}

constexpr int stderr_fileno = 2;

void error_write(cstring msg){
	write(stderr_fileno, msg, cstring_len(msg));
}

[[noreturn]]
void trap(){
	abort();
	for(;;);
}

//// FT_Sched platform specifics
#include "ft_sched.hpp"

struct RawTaskPlatformSpecific {
	pthread_t handle;
	Atomic<bool> handle_initialized = false;
};

static
void* task_linux_wrapper(void* task_ptr){
	RawTask* task = (RawTask*)task_ptr;
	task->_status.store(TaskStatus_Started);

	task->func(task);

	task->_status.store(TaskStatus_Done);
	return NULL;
}

	// bool _platform_join();
	// bool _platform_cancel();

bool RawTask::_platform_init(Arena*, usize, RawTaskFunc, void*){
	if(_status.load() != TaskStatus_Initialized){
		// TODO: LOG: "Invalid task status";
		_status.store(TaskStatus_Fault);
		return false;
	}

	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	auto ok = pthread_create(&specific->handle, nullptr, task_linux_wrapper, (void*)this) == 0;
	specific->handle_initialized.store(true);

	return ok;
}

bool RawTask::_platform_join(){
	auto status = this->_status.load(memory_order_relaxed);
	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);

	if(status >= TaskStatus_Done && !specific->handle_initialized.load(memory_order_relaxed)){
		// Joined already
		return true;
	}

	auto res = pthread_join(specific->handle, NULL) == 0;
	specific->handle_initialized.store(false);
	status = this->_status.load();
	ensure(status == TaskStatus_Fault || status == TaskStatus_Done, "Invalid task status");
	return true;
}

bool RawTask::_platform_cancel(){
	auto status = _status.load(memory_order_relaxed);
	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);

	if(status >= TaskStatus_Done || !specific->handle_initialized){
		// Already cancelled
		return true;
	}

	bool res = true;
	if(_status.load(memory_order_relaxed) == TaskStatus_Started){
		res = pthread_cancel(specific->handle) == 0;
		specific->handle_initialized.store(false);
	}
	_status.store(TaskStatus_Fault);
	return res;
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
