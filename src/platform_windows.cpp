#define WIN32_LEAN_AND_MEAN
#include "ft_sched.hpp"
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

usize _tick_frequency = 0;

TimeTick tick_now(){
	LARGE_INTEGER t {};
	static_assert(sizeof(usize) >= sizeof(t.QuadPart), "Not implemented on windows 32 bit");

	if(!QueryPerformanceCounter(&t)){
		panic("Failed to read perf counter");
	}
	return t.QuadPart;
}

usize tick_frequency(){
	if(!_tick_frequency){
		LARGE_INTEGER f{};
	    if(!QueryPerformanceFrequency(&f)){
	    	panic("Failed to get frequency");
	    }

		_tick_frequency = f.QuadPart;
	}

	return _tick_frequency;
}

TimeTick duration_to_tick(Duration d){
	auto ticks_per_duration = f64(tick_frequency()) / f64(Duration::scale);
	auto tick_count = usize(f64(d._value) * ticks_per_duration);

	return tick_count;
}

Duration tick_diff(TimeTick a, TimeTick b){
	f64 diff = f64(a) - f64(b);
	f64 tick_duration = f64(Duration::scale) / f64(tick_frequency());
	auto nano_diff = static_cast<isize>(diff * tick_duration);
	return {nano_diff};
}


void sleep_for(Duration d){
	constexpr f64 milliseconds_per_duration = 1000.0 / static_cast<f64>(Duration::scale);

	DWORD ms = max(1, (d._value / Duration::millisecond));
	Sleep(ms);
}

