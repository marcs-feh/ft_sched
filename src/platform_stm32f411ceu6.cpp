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

struct RawTaskPlatformSpecific {
	alignas(portBYTE_ALIGNMENT) TaskHandle_t handle;
	alignas(portBYTE_ALIGNMENT) void* stack_base;
	u32 stack_size;
};

constexpr usize rtos_stack_size_words = 200;

static
void _freertos_task_wrapper(void* task_ptr){
	RawTask* task = (RawTask*)task_ptr;
	auto specific = (RawTaskPlatformSpecific*)(&task->_specific);

	task->_status.store(TaskStatus_Started);
	printf("ENTER TASK %d\r\n", int(task->id));
	task->func(task);
	printf("FINISH TASK %d\r\n", int(task->id));
	task->_status.store(TaskStatus_Done);

	/* Yield spin */
	while(1){
		taskYIELD();
	}
}

void RawTask::_init_specifics_and_run(){
	if(_status.load() != TaskStatus_Initialized){
		// TODO: log fail
		_status.store(TaskStatus_Fault);
		return;
	}

	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);

	auto name = arena_printf(arena, "T:%d", int(id));
	auto tcb = make<StaticTask_t>(arena);
	auto stack = (StackType_t*)arena->alloc(rtos_stack_size_words * sizeof(StackType_t), portBYTE_ALIGNMENT * 2);

	if(!name.data){
		panic("Failed to allocate name");
	}
	if(!tcb){
		panic("Failed to allocate TCB");
	}
	if(!stack){
		panic("Failed to allocate stack");
	}

	TaskHandle_t handle = xTaskCreateStatic(
		_freertos_task_wrapper, /* Task body */
		name.data, /* Name */
		rtos_stack_size_words, /* Stack size */
		(void*)this, /* Task Parameter */
		osPriorityNormal, /* Priority */
		stack, /* Stack data */
		tcb /* TCB data */
	);

	specific->handle = handle;
}

void RawTask::_join_and_deinit_specifics(){
	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	TaskHandle_t handle = specific->handle;

	for(
		auto status = this->_status.load(memory_order_relaxed);
		status < TaskStatus_Done;
		status = this->_status.load(memory_order_relaxed)
	){
		printf("Joining from %d (status=%d)\r\n", int(id), int(status)); fflush(stdout);
		sleep_for(Duration::from_milli(250));
	}

	vTaskDelete(handle);
	printf("DELETED: %d\r\n", int(id));

	// if(specific.handle)
	// 	vTaskDelete(specific.handle);
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
