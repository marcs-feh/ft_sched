#include "FreeRTOS.h"

//// Base platform specifics
#include "base.hpp"

struct RawTaskPlatformSpecific {
	BaseType_t _handle;
};

static
void _freertos_task_wrapper(void* arg){
}


 // BaseType_t xTaskCreate( TaskFunction_t pvTaskCode,
 //                         const char * const pcName,
 //                         const configSTACK_DEPTH_TYPE uxStackDepth,
 //                         void *pvParameters,
 //                         UBaseType_t uxPriority,
 //                         TaskHandle_t *pxCreatedTask
 //                       );


//// FT_Sched platform specifics
