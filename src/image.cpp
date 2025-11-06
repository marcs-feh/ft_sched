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

void save_p5(Bitmap const& bmp, IO_Writer writer){
	u8 header_buf[32];

	String header = buffer_printf({&header_buf[0], sizeof(header_buf)}, "P5\n%d %d\n255\n", int(bmp.width), int(bmp.height));
	writer.write(header.raw_bytes());
	writer.write(bmp.pixel_data);
}

Option<Bitmap> load_p5(IO_Reader reader){
	constexpr cstring magic = "P5\n";
	u8 file_magic[3];

	auto magic_len = reader.read({&file_magic[0], 3});
	if(magic_len != 3){
		return {};
	}

	if(mem_compare(magic, &file_magic[0], 3) != 0){
		return {};
	}

	/* Read width */ {
		Array<u8, 24> dimension_data = {};
		auto buf = dimension_data.slice();
		usize current = 0;
		
		reader.read_line(dimension_data.slice());
	}

	unimplemented();
}


