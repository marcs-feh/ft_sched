#include "base.hpp"

struct Rect {
	i32 x, y;
	i32 w, h;

	bool valid() const {
		return (w > 0) && (h > 0);
	}

	i32 area() const {
		return w * h;
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

	bool operator==(Rect const& r) const {
		return (r.x == x) && (r.y == y) && (r.w == w) && (r.h == h);
	}

	bool operator!=(Rect const& r) const {
		return !(r == *this);
	}
};

template<typename F>
concept PixelFunc = requires(F f, u8 px){
	{ f(px) } -> SameAs<u8>;
};

struct Bitmap {
	Slice<u8> pixel_data;
	u32 width;
	u32 height;

	template<PixelFunc Func>
	void apply_pixel_transform(Func&& f){
		for(usize i = 0; i < pixel_data; i += 1){
			pixel_data[i] = f(pixel_data[i]);
		}
	}

	// Copy a rectangle from image onto arena, the outside region of the rectangle is filled with a provided default value
	// Option<Slice<RGBA8>> copy_region_padded(Arena* a, Rect rect, RGBA8 fill_value);

	// Copy a rectangle from image onto arena
	Option<Slice<u8>> copy_region(Arena* a, Rect rect);

	Rect bounds() const {
		return {
			.x = 0, .y =0,
			.w = i32(width), .h = i32(height),
		};
	}

	Bitmap()
		: pixel_data{}
		, width{0}
		, height{0}
	{}
};


Option<Slice<u8>> Bitmap::copy_region(Arena* a, Rect rect){
	auto intersection = rect.intersect(this->bounds());
	auto inside = intersection == rect;
	if(!inside){
		return {};
	}

	auto dest = make_slice<u8>(a, rect.area());
	if(!dest){
		return {};
	}

	i32 y0 = rect.y;
	i32 y1 = rect.y + rect.h;
	i32 source_stride = this->width;
	i32 dest_stride = rect.w;

	for(isize y = y0; y < y1; y += 1){
		auto source_row = this->pixel_data.skip(y0 * source_stride).take(rect.w);
		auto dest_row = dest.skip(y0 * dest_stride).take(rect.w);
		copy(dest_row, source_row);
	}

	return dest;
}

struct Bitmap_Header {
	u32 width;
	u32 height;
};

void bitmap_write(Bitmap const& bmp, IO_Writer w){
	Bitmap_Header header;
	header.width = bmp.width;
	header.height = bmp.height;

	u8 buf[sizeof(header)];

	auto written = w.write({buf, sizeof(buf)});
	ensure(written == sizeof(header), "Failed to write");

	written = w.write(bmp.pixel_data);
	ensure(written == bmp.pixel_data.len, "Failed to write");
}

Option<Bitmap> bitmap_load(Slice<u8> data){
	if(data.len < sizeof(Bitmap_header)){
		return {};
	}

	Bitmap_Header header;
	auto header_mem = data.take(sizeof(Bitmap_Header));
	data = data.skip(sizeof(Bitmap_Header));

	mem_copy(&header, header_mem.data, sizeof(Bitmap_Header));

	auto pixel_count = header.width * header.height;
	if(data.len < pixel_count){
		return {};
	}
	auto pixels = data.take(pixel_count);


	Bitmap bmp;
	bmp.pixel_data = pixels;
	bmp.width = header.width;
	bmp.height = header.height;
	return bmp;
}

