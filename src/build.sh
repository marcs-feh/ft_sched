# Auto generated at 2025-10-25 21:57:40
set -eu
mkdir -p build
arm-none-eabi-g++ -std=c++20 -fwrapv -fno-strict-aliasing -fno-rtti -fno-exceptions -I. -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -ffunction-sections -fdata-sections -nostdlib -I./stm32/Core/Inc -I./stm32/Middlewares/Third_Party/FreeRTOS/Source/include -I./stm32/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I./stm32/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -DFT_SCHED_NO_MAIN -Os -Wall -Wextra -Werror=return-type -c main.cpp -o build/main.cpp.o &
arm-none-eabi-g++ -std=c++20 -fwrapv -fno-strict-aliasing -fno-rtti -fno-exceptions -I. -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -ffunction-sections -fdata-sections -nostdlib -I./stm32/Core/Inc -I./stm32/Middlewares/Third_Party/FreeRTOS/Source/include -I./stm32/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I./stm32/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -DFT_SCHED_NO_MAIN -Os -Wall -Wextra -Werror=return-type -c platform_stm32f411ceu6.cpp -o build/platform_stm32f411ceu6.cpp.o &
wait
arm-none-eabi-ar rcs build/libft_sched.a build/main.cpp.o build/platform_stm32f411ceu6.cpp.o
echo Finished [Release/Stm32blackpill]

