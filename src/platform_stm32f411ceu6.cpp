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
	alignas(portBYTE_ALIGNMENT) TaskHandle_t handle;
	char name[task_name_size];
};

constexpr usize rtos_stack_size_words = 200;

static inline
RawTaskPlatformSpecific read_specifics(RawTask* t, ){
	RawTaskPlatformSpecific out;
	mem_set(&out, 0, sizeof(out))
	mem_copy(&out, &t->specifics, sizeof(out));
	return out;
}

static
void _freertos_task_wrapper(void* task_ptr){
	RawTask* task = (RawTask*)task_ptr;
	auto specific = read_specifics(this);

	task->_status.store(TaskStatus_Started);
	printf("ENTER TASK %d\r\n", int(task->id));
	task->func(task);
	printf("FINISH TASK %d\r\n", int(task->id));
	task->_status.store(TaskStatus_Done);
	// vTaskDelete(NULL); // IMPORTANT: Self-Deinitialize task

	/* Yield spin */
	while(1){
		osDelay(0);
	}
}

void RawTask::_init_specifics_and_run(){
	if(_status.load() != TaskStatus_Initialized){
		// TODO: log fail
		_status.store(TaskStatus_Fault);
		return;
	}

	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);

	auto namebuf = Slice<u8>{(u8*)&specific->name[0], sizeof(task_name_size)};
	auto name = buffer_printf(namebuf, "T:%d", int(id));

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
	// auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	auto specific = read_specifics(this);

	for(
		auto status = this->_status.load(memory_order_relaxed);
		status != TaskStatus_Fault && status != TaskStatus_Done;
		status = this->_status.load(memory_order_relaxed)
	){
		printf("Joining %d (status=%d)\r\n", int(id), int(status)); fflush(stdout);
		sleep_for(Duration::from_milli(250));
	}

	if(specific->handle)
		vTaskDelete(specific->handle);
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
static_assert(alignof(RawTaskPlatformSpecific) <= alignof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient alignment");
