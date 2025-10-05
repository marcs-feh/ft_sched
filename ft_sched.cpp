/// Library TU
#include "ft_sched.hpp"
#include "task.hpp"

#include "task.cpp"
#include "crc32.gen.cpp"

#if defined(TARGET_HOSTED_LINUX)
	#include "task_linux.cpp"
#elif defined(TARGET_HOSTED_WINDOWS)
	#include "task_windows.cpp"
#else
	#error "Specify target platform"
#endif
