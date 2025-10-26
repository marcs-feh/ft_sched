# Auto generated at 2025-10-25 21:49:28
set -eu
mkdir -p build
clang++ -std=c++20 -fwrapv -fno-strict-aliasing -fno-rtti -fno-exceptions -I. -D_CRT_SECURE_NO_WARNINGS -g3 -O0 -Wall -Wextra -Werror=return-type -c main.cpp -o build/main.cpp.o &
clang++ -std=c++20 -fwrapv -fno-strict-aliasing -fno-rtti -fno-exceptions -I. -D_CRT_SECURE_NO_WARNINGS -g3 -O0 -Wall -Wextra -Werror=return-type -c platform_windows.cpp -o build/platform_windows.cpp.o &
wait
clang++ -o build/ft_sched.exe build/main.cpp.o build/platform_windows.cpp.o
echo Finished

