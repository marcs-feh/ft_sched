set -xeu

BinName="mcu"

arm-none-eabi-objcopy -O binary build/"$BinName".elf build/"$BinName".bin
st-flash write build/"$BinName".bin 0x08000000


