#include "base.hpp"

struct Rect {
	i32 x, y;
	i32 w, h;
};

using RGBA8 = Array<u8, 4>;

template<typename F>
concept PixelFunc = requires(F f, RGBA8 px){
	{ f(px) } -> SameAs<RGBA8>;
};

struct Bitmap {
	Slice<RGBA8> pixel_data;
	u32 width;
	u32 height;
	u8 channels;

	template<PixelFunc Func>
	void apply_pixel_transform(Func&& f){
		for(usize i = 0; i < pixel_data; i += 1){
			pixel_data[i] = f(pixel_data[i]);
		}
	}

	// Copy a rectangle from image onto arena, the outside region of the rectangle is filled with a provided default value
	Option<Slice<RGBA8>> copy_region_padded(Arena* a, Rect rect, RGBA8 fill_value);

	// Copy a rectangle from image onto arena
	Option<Slice<RGBA8>> copy_region_padded(Arena* a, Rect rect);

	Bitmap()
		: pixel_data{}
		, width{0}
		, height{0}
		, channels{0}
	{}
};

// Take a rectangle piece of source and copy it onto dest at position. Returns if it could be done
// bool bitmap_copy_rect(Bitmap* dest, Array<i32, 2> dest_pos, Bitmap const& source, Rect source_part);

