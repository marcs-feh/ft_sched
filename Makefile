CC := clang++
CFLAGS := -std=c++20 -fno-exceptions -fno-rtti -fno-strict-aliasing -fwrapv
WFLAGS := -Wall -Wextra -Werror=return-type -D_CRT_SECURE_NO_WARNINGS
LDFLAGS :=

.PHONY: run clean

all: ft_sched.exe
	@./ft_sched.exe

base.o: base.cpp base.hpp
	$(CC) $(CFLAGS) $(WFLAGS) -c base.cpp -o base.o

generate.o: generate.cpp
	$(CC) $(CFLAGS) $(WFLAGS) -c generate.cpp -o generate.o

ft_sched.o: main.cpp generate.exe
	./generate.exe
	$(CC) $(CFLAGS) $(WFLAGS) -c main.cpp -o ft_sched.o

generate.exe: generate.o base.o
	$(CC) generate.o base.o -o generate.exe $(LDFLAGS)

ft_sched.exe: ft_sched.o base.o
	$(CC) ft_sched.o base.o -o ft_sched.exe $(LDFLAGS)

clean:
	rm -f *.o *.exe *.pdb *.ilk