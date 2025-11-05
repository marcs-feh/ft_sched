#include "base.hpp"

struct Rect {
	i32 x, y;
	i32 w, h;

	bool valid() const {
		return (w > 0) && (h > 0);
	}

	Rect intersect(Rect b) const {
		const auto a = *this;

		auto ix0 = max(a.x, b.x);
		auto ix1 = min(a.x + a.w, b.x + b.w);

		auto iy0 = max(a.y, b.y);
		auto iy1 = min(a.y + a.h, b.y + b.h);

		return Rect {
			.x = ix0, .y = iy0,
			.w = ix1 - ix0, .h = iy1 - iy0,
		};
	}

	Rect join(Rect b) const {
		const auto a = *this;

		auto ix0 = min(a.x, b.x);
		auto ix1 = max(a.x + a.w, b.x + b.w);

		auto iy0 = min(a.y, b.y);
		auto iy1 = max(a.y + a.h, b.y + b.h);

		return Rect {
			.x = ix0, .y = iy0,
			.w = ix1 - ix0, .h = iy1 - iy0,
		};
	}
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
	// Option<Slice<RGBA8>> copy_region_padded(Arena* a, Rect rect, RGBA8 fill_value);

	// Copy a rectangle from image onto arena
	Option<Slice<RGBA8>> copy_region(Arena* a, Rect rect);

	Bitmap()
		: pixel_data{}
		, width{0}
		, height{0}
		, channels{0}
	{}
};


Option<Slice<RGBA8>> Bitmap::copy_region(Arena* a, Rect rect){
	// auto out_of_bounds = (rect.x < 0) || (rect.x >= this->width) || (rect.y < 0) || (rect.y >= this->height);
	// if(out_of_bounds){
	// 	return {};
	// }

	// rect.
	unimplemented();
}

// Take a rectangle piece of source and copy it onto dest at position. Returns if it could be done

