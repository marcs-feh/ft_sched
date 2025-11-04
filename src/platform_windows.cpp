#define WIN32_LEAN_AND_MEAN
#include "ft_sched.hpp"
#include <windows.h>
#include <stdio.h>

//// Base platform specifics
extern "C" {
void abort();
#include <stdio.h>
}

void error_write(cstring msg){
	fputs(msg, stderr);
}

[[noreturn]]
void trap(){
	abort();
	for(;;);
}

//// FT_Sched platform specifics
struct RawTaskPlatformSpecific {
	Atomic<HANDLE> handle;
	DWORD id;
};

static_assert(Atomic<HANDLE>::is_always_lock_free, "Atomic handle is not lock free");

static
DWORD task_windows_wrapper(LPVOID arg){
	auto task = (RawTask*)arg;
	task->_status.store(TaskStatus_Started);

	task->func(task);

	task->_status.store(TaskStatus_Done);
	return 0;
}

bool RawTask::_platform_init(Arena*, usize stack_size, RawTaskFunc, void*){
	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);

	constexpr usize min_stack_size = 64 * 1024;

	if(_status.load() != TaskStatus_Initialized){
		// TODO: LOG: "Invalid task status";
		_status.store(TaskStatus_Fault);
		return false;
	}

	DWORD id;
	HANDLE handle = CreateThread(NULL, max(min_stack_size, stack_size), task_windows_wrapper, (LPVOID)this, 0, &id);
	specific->handle.store(handle, memory_order_seq_cst);

	if(handle == NULL){
		return false;
	}
	return true;
}

bool RawTask::_platform_join(){
	auto status = this->_status.load();
	if(status == TaskStatus_Fault){
		return false;
	}

	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);

	if(status >= TaskStatus_Done && specific->handle == NULL){
		// Already joined
		return true;
	}

	DWORD ret = WaitForSingleObject(specific->handle, INFINITE);

	if(ret == WAIT_FAILED){
		panic("wait failed");
		return false;
	}

	status = this->_status.load();
	ensure(status >= TaskStatus_Done, "Invalid task status");

	auto closed = CloseHandle(specific->handle);
	if(!closed){
		panic("close failed");
		return false;
	}

	specific->handle.store(NULL, memory_order_seq_cst);

	return true;
}

bool RawTask::_platform_cancel(){
	auto status = this->_status.load(memory_order_relaxed);
	if(status >= TaskStatus_Done){
		return false;
	}

	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	this->_status.store(TaskStatus_Fault);
	auto terminated = TerminateThread(specific->handle, 0) != 0;
	if(!terminated){
		return false;
	}
	auto closed = CloseHandle(specific->handle) != 0;
	if(!closed){
		return false;
	}

	specific->handle = NULL;

	return true;
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
		ensure((_tick_frequency / Duration::scale) > 0, "Tick frequency is too low to be accurately represented");
	}

	return _tick_frequency;
}

void task_yield(){
	Sleep(0);
}

void sleep_for(Duration d){
	DWORD ms = max(0, (d._value / Duration::millisecond));
	Sleep(ms);
}

