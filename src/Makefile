# Auto generated at 2025-10-25 21:18:20
CC = clang++
AR = ar
CFLAGS = -std=c++20 -fwrapv -fno-strict-aliasing -fno-rtti -fno-exceptions -I. -D_CRT_SECURE_NO_WARNINGS -g3 -O0
WFLAGS = -Wall -Wextra -Werror=return-type
LDFLAGS = 

.PHONY: clean all

all: ft_sched.exe Makefile

-include main.cpp.d
main.cpp.o: main.cpp
	$(CC) $(CFLAGS) $(WFLAGS) -c main.cpp -MMD -MF main.cpp.d -o main.cpp.o

-include base.cpp.d
base.cpp.o: base.cpp
	$(CC) $(CFLAGS) $(WFLAGS) -c base.cpp -MMD -MF base.cpp.d -o base.cpp.o

-include ft_sched.cpp.d
ft_sched.cpp.o: ft_sched.cpp
	$(CC) $(CFLAGS) $(WFLAGS) -c ft_sched.cpp -MMD -MF ft_sched.cpp.d -o ft_sched.cpp.o

-include platform_windows.cpp.d
platform_windows.cpp.o: platform_windows.cpp
	$(CC) $(CFLAGS) $(WFLAGS) -c platform_windows.cpp -MMD -MF platform_windows.cpp.d -o platform_windows.cpp.o

ft_sched.exe: main.cpp.o base.cpp.o ft_sched.cpp.o platform_windows.cpp.o
	$(CC) -o ft_sched.exe $(LDFLAGS) main.cpp.o base.cpp.o ft_sched.cpp.o platform_windows.cpp.o
clean:
	busybox rm -f main.cpp.o base.cpp.o ft_sched.cpp.o platform_windows.cpp.o main.cpp.d base.cpp.d ft_sched.cpp.d platform_windows.cpp.d ft_sched.exe

