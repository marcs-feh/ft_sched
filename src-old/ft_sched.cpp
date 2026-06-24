/// Library Translation Unit
#include "ft_sched.hpp"

#include "raw_task.cpp"
#include "crc32.cpp"
#include "deadline.cpp"
#include "software_watchdog.cpp"

#if defined(FT_SCHED_PLATFORM_LINUX)
	#include "platform_linux.cpp"
#elif defined(FT_SCHED_PLATFORM_WINDOWS)
	#include "platform_windows.cpp"
#elif defined(FT_SCHED_PLATFORM_STM32F411CEU6)
	#include "platform_stm32f411ceu6.cpp"
#else
	#error "You must define a platform macro"
#endif

