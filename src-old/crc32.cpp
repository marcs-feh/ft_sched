#include "base.hpp"
#include "crc32_lut.gen.cpp"

u32 CRC32<Slice<u8>>::get(Slice<u8> buf){
    const volatile u8* raw_data = reinterpret_cast<const volatile u8*>(buf.data);

	constexpr u32 bit_width = sizeof(u32) * 8;

    u8 cur_byte = 0;
    u32 remainder = 0;

    for (usize i = 0; i < buf.len; ++i) {
        cur_byte = raw_data[i] ^ (remainder >> (bit_width - 8));
        remainder = crc32_lut[cur_byte] ^ (remainder << 8);
    }

    return remainder;
}

u32 crc32_advance(u32 current, Slice<u8> buf){
    constexpr u32 bit_width = sizeof(u32) * 8;

    u8 cur_byte = 0;

    for(usize i = 0; i < buf.len; ++i){
        cur_byte = buf.data[i] ^ (current >> (bit_width - 8));
        current = crc32_lut[cur_byte] ^ (current << 8);
    }

    return current;
}
