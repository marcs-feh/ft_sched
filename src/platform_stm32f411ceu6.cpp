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
#include "task.hpp"

struct RawTaskPlatformSpecific {
	TaskHandle_t handle;
};

constexpr usize rtos_stack_size_words = 200;

static
void _freertos_task_wrapper(void* task_ptr){
	RawTask* task = (RawTask*)task_ptr;
	task->_status.store(TaskStatus_Started);
	task->func(task);
	task->_status.store(TaskStatus_Done);
}

void RawTask::_init_specifics_and_run(){
	ensure(_status.load() == TaskStatus_Initialized, "Invalid task status");

	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);

	BaseType_t ok = xTaskCreate(
		_freertos_task_wrapper,
		"Task",
		rtos_stack_size_words,
		(void*)this,
		tskIDLE_PRIORITY,
		&specific->handle
	) == pdPASS;

	ensure(ok, "Failed to create thread");
}

void RawTask::_join_and_deinit_specifics(){
	unimplemented();
}

constexpr u32 MAX_DELAY = 1'000'000'000;

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
