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
	Option<Bitmap> copy_region(Arena* a, Rect rect);

	Option<Bitmap> copy_region_padded(Arena* a, Rect rect, u8 val);

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

Option<Bitmap> Bitmap::copy_region(Arena* a, Rect rect){
	auto intersection = rect.intersect(this->bounds());
	auto inside = intersection == rect;
	if(!inside){
		return {};
	}

	auto dest = make_slice<u8>(a, rect.area());
	if(!dest){
		return {};
	}

	Bitmap bmp;
	bmp.width = rect.w;
	bmp.height = rect.h;
	bmp.pixel_data = dest;

	i32 y0 = rect.y;
	i32 y1 = rect.y + rect.h;
	i32 source_stride = this->width;
	i32 dest_stride = rect.w;

	for(isize y = y0; y < y1; y += 1){
		auto source_row = this->pixel_data.skip((y * source_stride) + rect.x).take(rect.w);
		auto dest_row = dest.skip((y - y0) * dest_stride).take(rect.w);
		copy(dest_row, source_row);
	}

	return bmp;
}

Option<Bitmap> Bitmap::copy_region_padded(Arena* a, Rect rect, u8 val){
	auto intersection = rect.intersect(this->bounds());

	auto dest = make_slice<u8>(a, rect.area());
	if(!dest){
		return {};
	}
	mem_set(dest.data, val, dest.len);

	Bitmap bmp;
	bmp.width = rect.w;
	bmp.height = rect.h;
	bmp.pixel_data = dest;

	i32 y0 = intersection.y;
	i32 y1 = intersection.y + intersection.h;
	i32 source_stride = this->width;
	i32 dest_stride = rect.w;

	i32 xd = rect.x - intersection.x;
	i32 yd = rect.y - intersection.y;
	for(isize y = y0; y < y1; y += 1){
		auto source_row = this->pixel_data.skip((y * source_stride) + intersection.x).take(intersection.w);
		auto dest_row = dest.skip((y - y0 - yd) * dest_stride - xd).take(intersection.w);
		copy(dest_row, source_row);
	}

	return bmp;
}

struct Bitmap_Header {
	u32 width;
	u32 height;
};

static inline
bool is_digit(u8 c){
	return (c >= '0') && (c <= '9');
}

Option<i32> parse_i32(String s){

	usize digits_len = 0;
	for(; digits_len < s.len; digits_len++){
		if(!is_digit(s[digits_len])){
			break;
		}
	}

	auto digits = s.take(digits_len);
	if(digits.len == 0){
		return {};
	}

	i32 acc = 0;
	i32 power = 1;

	for(usize i = 0; i < s.len; i += 1){
		auto rpos = s.len - (i + 1);
		acc += (digits[rpos] - '0') * power;
		power *= 10;
	}

	return Option(acc);
}

Option<Bitmap> load_p5(Slice<u8> data){
	auto magic = String(data.take(3));
	if(magic != "P5\n"){
		return {};
	}
	data = data.skip(3);

	i32 width = 0;
	i32 height = 0;

	{
		usize width_end = data.linear_search([](u8 b){
			return b == ' ';
		}).unwrap_or(0);

		if(!width_end){
			return {};
		}

		auto width_str = String(data.take(width_end));
		width = parse_i32(width_str).unwrap_or(0);
		if(!width){
			return {};
		}

		data = data.skip(width_end);
	}

	data = data.skip(1); // space

	{
		usize height_end = data.linear_search([](u8 b){
			return b == '\n';
		}).unwrap_or(0);

		if(!height_end){
			return {};
		}

		auto height_str = String(data.take(height_end));
		height = parse_i32(height_str).unwrap_or(0);
		if(!height){
			return {};
		}

		data = data.skip(height_end);
	}

	data = data.skip(5); // \n255\n

	if(data.len < (width * height)){
		return {};
	}

	Bitmap bmp;
	bmp.height = height;
	bmp.width = width;
	bmp.pixel_data = data.take(width * height);

	return bmp;
}

void save_p5(Bitmap const& bmp, IO_Writer w){
	Array<u8, 32> headerbuf;
	auto header = buffer_printf(headerbuf.slice(), "P5\n%d %d\n255\n", int(bmp.width), int(bmp.height));
	w.write(header.raw_bytes());
	w.write(bmp.pixel_data);
}
