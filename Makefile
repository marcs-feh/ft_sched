CC := clang++
CFLAGS := -std=c++20 -fno-exceptions -fno-rtti -fno-strict-aliasing -fwrapv
WFLAGS := -Wall -Wextra -Werror=return-type -D_CRT_SECURE_NO_WARNINGS
OPTFLAGS := -O0
LDFLAGS :=

.PHONY: run clean

all: ft_sched.exe
	@./ft_sched.exe

generate.exe: generate.cpp base.cpp base.hpp
	# Code generation does not really need to be optimized
	$(CC) $(CFLAGS) $(WFLAGS) -O0 -g generate.cpp base.cpp -o generate.exe $(LDFLAGS)

ft_sched.exe: main.cpp base.cpp base.hpp generate.exe
	./generate.exe
	$(CC) $(CFLAGS) $(OPTFLAGS) $(WFLAGS) main.cpp base.cpp -o ft_sched.exe $(LDFLAGS)

clean:
	rm -f *.o *.exe *.pdb *.ilk
