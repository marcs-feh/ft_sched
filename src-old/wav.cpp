#include "base.hpp"

struct WAV_Header {
	char riff[4]; // Always "RIFF"
	u32 file_size; // Size of the file - 8
	char wave[4]; // Always "wave"
};

constexpr u16 wav_format_pcm = 1;

struct WAV_Format {
    char fmt[4]; // Always "fmt "
    u32 chunk_size;
    u16 audio_format;
    u16 num_channels;
    u32 sample_rate;
    u32 byte_rate; // sample_rate * num_channels * bits_per_sample/8
    u16 block_align; // num_channels * bits_per_sample/8
    u16 bits_per_sample; // 8, 16, etc.
};

struct WAV_Data {
	char data[4]; // Always "data"
	u32 data_size;
};

struct WAV_Info {
	u32 sample_rate;
	u16 num_channels;
    u32 num_samples;
    u8* audio_data;
};

Option<WAV_Info> wav_load_8bit_pcm(Slice<u8> buffer){
    constexpr u32 min_size = sizeof(WAV_Header) + sizeof(WAV_Format) + sizeof(WAV_Data);
    if (buffer.len < min_size) {
        return {};
    }

    u8* cursor = &buffer[0];
    u8* buffer_end = &buffer[0] + buffer.len;
    
    WAV_Header header = {};
    WAV_Format format = {};
    WAV_Data data = {};

    /* Load header */ {
        if (cursor + sizeof(header) > buffer_end) return {};
        mem_copy(&header, cursor, sizeof(header));
        cursor += sizeof(header);

        auto header_ok = (mem_compare(&header.riff[0], "RIFF", 4) == 0) 
                      && (mem_compare(&header.wave[0], "WAVE", 4) == 0);
        if (!header_ok) return {};
    }

    /* Load format */ {
        if (cursor + sizeof(format) > buffer_end) return {};
        mem_copy(&format, cursor, sizeof(format));
        cursor += sizeof(format);

        auto format_ok = (mem_compare(&format.fmt[0], "fmt ", 4) == 0)
            && (format.audio_format == wav_format_pcm)
            && (format.bits_per_sample == 8);

        if (!format_ok) return {};
    }

    /* Load data */ {
        if (cursor + sizeof(data) > buffer_end) return {};
        mem_copy(&data, cursor, sizeof(data));
        cursor += sizeof(data);

        auto bytes_remaining = uintptr(buffer_end) - uintptr(cursor);
        auto data_ok = (mem_compare(&data.data[0], "data", 4) == 0)
            && (data.data_size <= bytes_remaining);

        if (!data_ok) return {};
    }

    WAV_Info info = {};
    info.sample_rate = format.sample_rate;
    info.num_channels = format.num_channels;
    info.num_samples = data.data_size / format.num_channels;
    info.audio_data = cursor;

    return info;
}

Slice<u8> wav_read(const WAV_Info *info, Slice<u8> dest, u32 sample_start, u32 sample_end) {
    if (sample_start >= info->num_samples || sample_end >= info->num_samples) {
        return {};
    }
    
    if (sample_start > sample_end) {
        return {};
    }
    
    u32 num_samples = sample_end - sample_start + 1;
    u32 bytes_to_copy = num_samples * info->num_channels;
    u32 start_offset = sample_start * info->num_channels;
    
    mem_copy(dest.data, info->audio_data + start_offset, bytes_to_copy);

    auto res = dest.take(bytes_to_copy);
    
    return res;
}