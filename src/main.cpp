#include <stdio.h>
#include <math.h>

#include "base.hpp"

#include "ft_sched.hpp"

#include "image.cpp"

template<int N>
struct Convolution_Context {
	Array<f32, N * N> kernel;
	Bitmap input;
	Arena* scratch;

	Rect rect_of(i32 x, i32 y){
		return {
			.x = x - N/2,
			.y = y - N/2,
			.w = N,
			.h = N,
		};
	}

	u8 get(i32 x, i32 y){
		auto r = rect_of(x, y);
		auto region = input.copy_region_padded(scratch, r, 0x00);
		if(!region){
			return 0;
		}
		auto data = region.unwrap().pixel_data;
		Array<f32, N * N> norm_data;

		ensure(data.len == (N*N), "Mismatched lengths");
		for(usize i = 0; i < data.len; i += 1){
			norm_data[i] = f32(data[i]) / 255.0f;
		}

		auto res = norm_data * kernel;
		f32 acc = 0;
		for(usize i = 0; i < (N * N); i += 1){
			acc += res[i];
		}

		scratch->reset();
		return u8(clamp<f32>(0, acc * 255, 255));
	}
};

Option<Slice<u8>> read_file_whole(cstring path, Arena* arena){
	FILE* f = fopen(path, "rb");
	if(!f){
		panic("FAIL TO OPEN");
		return {};
	}

	fseek(f, 0, SEEK_END);
	auto end = ftell(f);
	rewind(f);
	auto start = ftell(f);

	auto size = end - start;
	void* data = arena->alloc(size + 1, alignof(void*));
	if(!data){
		panic("FAIL TO ALLOC");
		fclose(f);
		return {};
	}

	auto n = fread(data, 1, size, f);

	fclose(f);

	return Slice<u8>{(u8*)data, n};
}

struct SystemStats {
	Atomic<i32> failed_assertions = 0;
	Atomic<i32> crc_failures = 0;
	Atomic<i32> total_stack_space = 0;
	Atomic<i32> total_arena_space = 0;

	void dump(){
		constexpr usize stat_dump_size = 512;
		static u8 stat_dump_memory[stat_dump_size];

		auto res = buffer_printf({&stat_dump_memory[0], stat_dump_size},
			"failed_assertions: %d\r\n"
			"crc_failures: %d\r\n"
			"total_stack_space: %d\r\n"
			"total_arena_space: %d\r\n"
			, int(failed_assertions.load()), int(crc_failures.load()), int(total_stack_space.load()), int(total_arena_space.load()));

		fflush(stdout);
		printf("%s\r\n", res.data);
		fflush(stdout);
	}
};

SystemStats sys_statistics;

extern "C" int puts(char const*);

template<class T>
void print_slice(Slice<T> slice, char const* elem_fmt){
	u8 elem_buf[32];
	printf("len: %td [ ", slice.len);
	for(usize i = 0; i < slice.len; i ++){
		String s = buffer_printf({&elem_buf[0], sizeof(elem_buf)}, elem_fmt, slice[i]);
		printf("%s ", s.data);
	} printf("]\r\n");
}

template<typename T>
using ConsensusFunc = bool (*)(T const&, T const&);

template<typename T>
int consensus(T& a, T& b, T& c, ConsensusFunc<T> f){
	auto ab = f(a, b);
	auto bc = f(b, c);
	auto ca = f(a, c);

	if(ab) return 0;
	if(bc) return 1;
	if(ca) return 2;

	// No consensus, uh oh!
	return -1;
}

template<typename T>
constexpr auto default_consensus_func = [](T const& a, T const& b) -> bool {
	return a == b;
};

Arena task_arena{};
Arena main_arena{};

constexpr usize max_task_count = 10;
constexpr usize average_stack_size = 200 * sizeof(usize);
constexpr usize task_arena_size = (max_task_count * average_stack_size) + 1024;

u8 task_arena_memory[task_arena_size];

constexpr usize main_arena_size = 4096 * 1024;
u8 main_arena_memory[main_arena_size];

void print_info(){
	static u8 bufdata[50];
	auto buf = Slice<u8>(&bufdata[0], sizeof(bufdata));
	String msg;

	msg = buffer_printf(buf, "[System Info]"); printf("%s\r\n", msg.data);
	msg = buffer_printf(buf, "  Task Arena Size: %zuB", task_arena_size); printf("%s\r\n", msg.data);
	msg = buffer_printf(buf, "  Address Width:   %zu-bit", sizeof(void*) * 8); printf("%s\r\n", msg.data);
	msg = buffer_printf(buf, "  Tick Frequency:  %tu Hz", tick_frequency()); printf("%s\r\n", msg.data);
	msg = buffer_printf(buf, "  RawTask size:    %td", sizeof(RawTask)); printf("%s\r\n", msg.data);
	// msg = buffer_printf(buf, "  WAV file size:   %td", sizeof(scattered_and_lost_wav_data)); printf("%s\r\n", msg.data);
}

constexpr f64 pi = 3.14159265358979323846264338327950288;
constexpr f64 tau = 2.0 * pi;
constexpr f64 euler = 2.71828182845904523536028747135266249;

constexpr f64 gaussian(f64 peak, f64 stddev, f64 x){
	auto exponent = - (x*x) / (2 * stddev * stddev);
	return peak * exp(exponent);
}


static inline
isize _io_stdout_func(u8 operation, void*, Slice<u8> buf){
	switch(operation){
	case io_operation_write:
		return fwrite(buf.data, 1, buf.len, stdout);

	case io_operation_read:
		return -1;

	case io_operation_close:
		return -1;

	case io_operation_peek:
		return -1;
	default:
		return -2;
	}
}

IO_Stream get_stdout_stream(){
	return {
		._impl = nullptr,
		._func = _io_stdout_func,
	};
}

static inline
isize _io_file_writer_func(u8 operation, void* handle, Slice<u8> buf){
	FILE* file = (FILE*)handle;
	switch(operation){
	case io_operation_write:
		return fwrite(buf.data, 1, buf.len, file);

	case io_operation_read:
		return -1;

	case io_operation_close:
		fclose(file);
		return 0;

	case io_operation_peek:
		return -1;
	default:
		return -2;
	}
}

IO_Writer get_file_writer(cstring path){
	FILE* f = fopen(path, "wb");
	ensure(f, "Failed to open");
	auto s = IO_Stream{
		._impl = (void*)f,
		._func = _io_file_writer_func,
	}
	;
	return s.as_writer();
}

void dump_bitmap(Bitmap const& bmap){
	auto writer = get_file_writer("out.pgm");
	save_p5(bmap, writer);
	writer.close();
}

__attribute__((never_inline)) static inline
void entrypoint(){
	main_arena = arena_from_buffer({&main_arena_memory[0], main_arena_size});
	task_arena = arena_from_buffer({&task_arena_memory[0], task_arena_size});

	#if defined(FT_SCHED_PLATFORM_STM32F411CEU6)
		for(int i = 5; i > 0; i --){
			sleep_for(Duration::from_milli(1'000)); printf("%d\r\n", i); fflush(stdout);
		}
		print_info();
	#else
		swdg_init(Duration::from_milli(1'000));
		DeadlineWatcher* watcher = make_deadline_watcher(&task_arena, 32);
		ensure(watcher != nullptr, "Failed to create watcher");
		auto watcher_task = make_basic_task(&task_arena, [watcher](TaskContext){
			while(1){
				auto ok = watcher->scan();
				if(ok){
					swdg_reset();
				}

				sleep_for(Duration::from_milli(1));
			}

			return Unit{};
		});
	#endif

	auto img_data = read_file_whole("lena.pgm", &main_arena).unwrap();

	auto bitmap = load_p5(img_data).unwrap();
	auto output = bitmap.copy(&main_arena).unwrap();

	Convolution_Context<3> conv;
	conv.input = bitmap;
	conv.scratch = main_arena.make_sub(1024);
	conv.kernel = Array<f32, 9>{
		-1.0f, 2.0f, -1.0f,
		0.0f, 0.0f, 0.0f,
		-1.0f, 2.0f, -1.0f,
	} / splat<f32, 9>(4.0f);

	for(usize x = 0; x < bitmap.width; x++){
		for(usize y = 0; y < bitmap.width; y++){
			output.pixel_data[(y * output.width) + x] = conv.get(x, y);
		}
	}

	conv.kernel = Array<f32, 9>{
		-1.0f, 0.0f, -1.0f,
		2.0f, 0.0f, 2.0f,
		-1.0f, 0.0f, -1.0f,
	} / splat<f32, 9>(4.0f);

	for(usize x = 0; x < bitmap.width; x++){
		for(usize y = 0; y < bitmap.width; y++){
			output.pixel_data[(y * output.width) + x] += conv.get(x, y);
		}
	}

	dump_bitmap(output);

	fflush(stdout);
	while(1){
		sleep_for({0});
		break;
	}
}

//// ---------------------------------------------
#if defined(FT_SCHED_NO_MAIN)
extern "C" void ft_sched_entrypoint()
#else
int main()
#endif
{
	entrypoint();
}

#include "base.cpp"
#include "ft_sched.cpp"

