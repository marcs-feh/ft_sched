u32 crc32(Slice<u8> buf){
	constexpr u32 bit_width = sizeof(u32) * 8;
    u8 data = 0;
    u32 remainder = 0;

    for (usize i = 0; i < buf.len; ++i) {
        data = buf[i] ^ (remainder >> (bit_width - 8));
        remainder = crc32_lut[data] ^ (remainder << 8);
    }

    return remainder;
}
