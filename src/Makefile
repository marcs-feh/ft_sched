# Auto generated at 2025-10-25 19:54:01
CC = arm-none-eabi-g++
AR = arm-none-eabi-ar
CFLAGS = -std=c++20 -fwrapv -fno-strict-aliasing -fno-rtti -fno-exceptions -I. -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -ffunction-sections -fdata-sections -fstack-usage -nostdlib -I./stm32/Core/Inc -I./stm32/Middlewares/Third_Party/FreeRTOS/Source/include -I./stm32/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I./stm32/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -g3 -O0
WFLAGS = -Wall -Wextra -Werror=return-type
LDFLAGS = 
.PHONY: clean all
all: libft_sched.a
-include main.cpp.d
main.cpp.o: main.cpp
	$(CC) $(CFLAGS) $(WFLAGS) -c main.cpp -MD -MF main.cpp.d -o main.cpp.o
-include base.cpp.d
base.cpp.o: base.cpp
	$(CC) $(CFLAGS) $(WFLAGS) -c base.cpp -MD -MF base.cpp.d -o base.cpp.o
-include ft_sched.cpp.d
ft_sched.cpp.o: ft_sched.cpp
	$(CC) $(CFLAGS) $(WFLAGS) -c ft_sched.cpp -MD -MF ft_sched.cpp.d -o ft_sched.cpp.o
-include platform_stm32f411ceu6.cpp.d
platform_stm32f411ceu6.cpp.o: platform_stm32f411ceu6.cpp
	$(CC) $(CFLAGS) $(WFLAGS) -c platform_stm32f411ceu6.cpp -MD -MF platform_stm32f411ceu6.cpp.d -o platform_stm32f411ceu6.cpp.o
libft_sched.a: main.cpp.o base.cpp.o ft_sched.cpp.o platform_stm32f411ceu6.cpp.o
	$(AR) rcs libft_sched.a main.cpp.o base.cpp.o ft_sched.cpp.o platform_stm32f411ceu6.cpp.o
	cp libft_sched.a stm32/libft_sched.a
clean:
	rm -f main.cpp.o base.cpp.o ft_sched.cpp.o platform_stm32f411ceu6.cpp.o main.cpp.d base.cpp.d ft_sched.cpp.d platform_stm32f411ceu6.cpp.d libft_sched.a

