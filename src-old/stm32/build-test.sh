set -eu
arm-none-eabi-g++ -std=c++20 -fwrapv -fno-strict-aliasing -fno-rtti -fno-exceptions -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -ffunction-sections -fdata-sections -fstack-usage -nostdlib -g3 -O0 -Wall -Wextra -Werror=return-type -c test.cpp
arm-none-eabi-ar rcs libtestcxx.a test.o
echo 'Done.'
