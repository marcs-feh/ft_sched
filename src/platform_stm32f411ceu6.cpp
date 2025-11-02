extern "C" {
	#include "FreeRTOS.h"
	#include "task.h"
	#include "cmsis_os2.h"
}

//// Base platform specifics
#include "base.hpp"

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
#include "ft_sched.hpp"

constexpr usize task_name_size = 12;

struct RawTaskPlatformSpecific {
	TaskHandle_t handle;
	char name[task_name_size];
};

constexpr usize rtos_stack_size_words = 200;

static
void _freertos_task_wrapper(void* task_ptr){
	RawTask* task = (RawTask*)task_ptr;
	task->_status.store(TaskStatus_Started);
	task->func(task);
	task->_status.store(TaskStatus_Done);
	vTaskDelete(NULL); // IMPORTANT: Self-Deinitialize task
}

void RawTask::_init_specifics_and_run(){
	if(_status.load() != TaskStatus_Initialized){
		// TODO: log fail
		_status.store(TaskStatus_Fault);
		return;
	}

	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);

	auto namebuf = Slice<u8>{(u8*)&specific->name[0], sizeof(task_name_size)}
	auto name = buffer_printf(namebuf, "T.%4d", namebuf);

	BaseType_t ok = xTaskCreate(
		_freertos_task_wrapper,
		name.data,
		rtos_stack_size_words,
		(void*)this,
		osPriorityNormal,
		&specific->handle
	) == pdPASS;

	ensure(ok, "Failed to create thread");
}

void RawTask::_join_and_deinit_specifics(){
	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	auto status = this->_status.load(memory_order_relaxed);
	while(status != TaskStatus_Fault && status != TaskStatus_Done){
		osDelay(0); // TODO: Is this actually correct?
	}
}

void RawTask::_cancel_and_deinit_specifics(){
	unimplemented();
	// auto status = this->_status.load(memory_order_relaxed);
	// if(status >= TaskStatus_Done){
	// 	return;
	// }

	// auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	// this->_status.store(TaskStatus_Fault);
	// auto terminated = TerminateThread(specific->handle, 0) != 0;
	// // ensure(terminated, "Failed to kill thread");
	// CloseHandle(specific->handle);
}

void sleep_for(Duration d){
	u32 ms = d.to_milli();
	osDelay(ms);
}

TimeTick tick_now(){
	return xTaskGetTickCount();
}

usize tick_frequency(){
	return configTICK_RATE_HZ;
}

static_assert(sizeof(RawTaskPlatformSpecific) <= sizeof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient size");
static_assert(alignof(RawTaskPlatformSpecific) <= alignof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient size");
