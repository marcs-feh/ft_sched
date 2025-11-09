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

// TODO: Loosen alignment
struct RawTaskPlatformSpecific {
	alignas(portBYTE_ALIGNMENT) Atomic<TaskHandle_t> handle;
	alignas(portBYTE_ALIGNMENT) void* stack_base;
	u32 stack_size;
};

constexpr usize platform_min_stack_size = sizeof(StackType_t) * configMINIMAL_STACK_SIZE;

static_assert(platform_min_stack_size >= task_min_stack_size, "Stack size is too small");

static
void _freertos_task_wrapper(void* task_ptr){
	RawTask* task = (RawTask*)task_ptr;
	auto specific = (RawTaskPlatformSpecific*)(&task->_specific);

	task->_status.store(TaskStatus_Started);
	task->func(task);
	task->_status.store(TaskStatus_Done);

	if(task->supervisor){
		task->supervisor->remove_key((void*)task);
	}

	/* IMPORTANT: Yield spin */
	while(1){
		taskYIELD();
	}
}

bool RawTask::_platform_init(Arena* arena, usize stack_size, RawTaskFunc, void*){
	if(_status.load() != TaskStatus_Initialized){
		_status.store(TaskStatus_Fault);
		return false;
	}

	stack_size = max(stack_size, platform_min_stack_size);

	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);

	auto name = arena_printf(arena, "T:%d", int(id));
	auto tcb = make<StaticTask_t>(arena);
	auto stack = (StackType_t*)arena->alloc(stack_size, portBYTE_ALIGNMENT * 2);

	if(!name.data){
		printf("Failed to allocate name\r\n");
		return false;
	}
	if(!tcb){
		printf("Failed to allocate TCB\r\n");
		return false;
	}
	if(!stack){
		printf("Failed to allocate stack: (offset=%d, capacity=%d, size=%d)\r\n", int(arena->offset), int(arena->capacity), int(stack_size));
		return false;
	}

	TaskHandle_t handle = xTaskCreateStatic(
		_freertos_task_wrapper, /* Task body */
		name.data, /* Name */
		stack_size / sizeof(StackType_t), /* Stack size */
		(void*)this, /* Task Parameter */
		osPriorityNormal, /* Priority */
		stack, /* Stack data */
		tcb /* TCB data */
	);

	specific->handle.store(handle);

	return handle != NULL;
}

bool RawTask::_platform_join(){
	constexpr auto join_interval = Duration::from_milli(5);

	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	TaskHandle_t handle = specific->handle.load();

	for(
		auto status = this->_status.load(memory_order_relaxed);
		status < TaskStatus_Done;
		status = this->_status.load(memory_order_relaxed)
	){
		// printf("Joining from %d (status=%d)\r\n", int(id), int(status)); fflush(stdout);
		sleep_for(join_interval);
	}

	if(handle){
		vTaskDelete(handle);
	}
	specific->handle.store(NULL);

	return true;
}

bool RawTask::_platform_cancel(){
	_status.store(TaskStatus_Fault);
	if(supervisor){
		supervisor->remove_key((void*)this);
	}

	auto specific = (RawTaskPlatformSpecific*)(&this->_specific);
	TaskHandle_t handle = specific->handle.load();

	if(handle){
		vTaskDelete(handle);
	}
	specific->handle.store(NULL);

	return true;
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

void task_yield(){
	taskYIELD();
}

static_assert(sizeof(RawTaskPlatformSpecific) <= sizeof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient size");
static_assert(alignof(RawTaskPlatformSpecific) <= alignof(RawTaskPlatformSpecificData), "Platform specific struct has insufficient alignment");
