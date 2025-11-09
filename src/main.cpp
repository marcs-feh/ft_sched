#include <stdio.h>
#include <math.h>

#include "base.hpp"

#include "ft_sched.hpp"

#include "image.cpp"

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

template<class T>
void print_slice(Slice<T> slice, char const* elem_fmt){
	u8 elem_buf[32];
	printf("len: %td [ ", slice.len);
	for(usize i = 0; i < slice.len; i ++){
		String s = buffer_printf({&elem_buf[0], sizeof(elem_buf)}, elem_fmt, slice[i]);
		printf("%s ", s.data);
	} printf("]\r\n");
}

Arena task_arena{};
Arena main_arena{};

constexpr usize max_task_count = 10;
constexpr usize average_stack_size = 200 * sizeof(usize);
constexpr usize task_arena_size = (max_task_count * average_stack_size) + 4096;

u8 task_arena_memory[task_arena_size];

constexpr usize main_arena_size = 40 * 1024;
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

#if defined(FT_SCHED_PLATFORM_STM32F411CEU6)
void dump_bitmap(Bitmap const& bmap){
	auto writer = get_stdout_stream().as_writer();
	writer.write(String("============ BEGIN BITMAP ==========\r\n").raw_bytes());
	save_p5(bmap, writer);
	writer.write(String("============ END BITMAP ============\r\n").raw_bytes());
	writer.close();
}
#else
void dump_bitmap(Bitmap const& bmap){
	auto writer = get_file_writer("out.pgm");
	save_p5(bmap, writer);
	writer.close();
}
#endif

#include "lena.pgm.cpp"

auto lena_image = load_p5(image_pgm_data).unwrap();

struct Point {
	i32 x;
	i32 y;
};

void convolve_sobel_worker(SPSC_Queue<Point>* input, SPSC_Queue<i32>* output){
	Convolution_Context<3> conv_horiz{};
	conv_horiz.input = lena_image;
	conv_horiz.scratch = main_arena.make_sub(100);
	conv_horiz.use_kernel(Array<f32, 9>{
		-1.0f, -2.0f, -1.0f,
		0.0f, 0.0f, 0.0f,
		+1.0f, +2.0f, +1.0f,
	} / splat<f32, 9>(4.0f));

	Convolution_Context<3> conv_vert = conv_horiz;
	conv_vert.scratch = main_arena.make_sub(100);
	conv_vert.use_kernel(Array<f32, 9>{
		-1.0f, 0.0f, +1.0f,
		-2.0f, 0.0f, +2.0f,
		-1.0f, 0.0f, +1.0f,
	} / splat<f32, 9>(4.0f));

	while(true){
		Point p = {0, 0};
		if(!input->pop_into(&p)){
			continue;
		}
		if(p.x < -999){ break; }

		auto hv = conv_horiz.get(p.x, p.y);
		auto vv = conv_vert.get(p.x, p.y);

		if(vv && hv){
			i32 r = clamp<i32>(0, hv.unwrap() + vv.unwrap(), 255);
			// printf("SEND: %d\n", r); fflush(stdout);
			output->push(r);
		}
	}
}

template<typename ...Args>
void log(cstring fmt, Args&& ...args){
	Array<u8, 72> buf;
	String res = buffer_printf(buf.slice(), fmt, forward<Args>(args)...);
	puts(res.data);
}

static inline
void entrypoint(){
	main_arena = arena_from_buffer({&main_arena_memory[0], main_arena_size});
	task_arena = arena_from_buffer({&task_arena_memory[0], task_arena_size});

	#if defined(FT_SCHED_PLATFORM_STM32F411CEU6)
		for(int i = 4; i > 0; i --){
			sleep_for(Duration::from_milli(1'000)); printf("%d\r\n", i); fflush(stdout);
		}
		print_info();
	#endif

	auto image = load_p5(image_pgm_data).unwrap("Failed to load image");
	bool running = true;

	auto output = lena_image.copy(&main_arena).unwrap("Failed to create output");
	ensure(output.pixel_data.len, "HUH?");

	auto sobel_input = make_spsc_queue<Point>(&main_arena, 4);
	auto sobel_output = make_spsc_queue<i32>(&main_arena, 50);

	auto sobel_task = make_basic_task(&task_arena, 1800, [sobel_input, sobel_output](TaskContext ctx) {
		convolve_sobel_worker(sobel_input, sobel_output);
		return Unit{};
	});

	auto begin = tick_now();
	for(i32 y = 0; y < lena_image.height; y++){
		for(i32 x = 0; x < lena_image.width; x++){
			sobel_input->push(Point{x, y});

			i32 res = -1;
			while(!sobel_output->pop_into(&res)){
			}

			if(res < 0){
				panic("Failed to get pixel");
			}
			output.pixel_data[(y * output.width) + x] = res;
		}
	}
	sobel_input->push(Point{-1000, -1000});

	auto end = tick_now();
	auto elapsed = tick_diff(end, begin);
	log("Took: %tdms", elapsed.to_milli());
	dump_bitmap(output);


	fflush(stdout);
	printf("------------------\r\n");

	#if defined(FT_SCHED_PLATFORM_STM32F411CEU6)
	while(1){
	}
	#endif
}

//// ---------------------------------------------
#if defined(FT_SCHED_NO_MAIN)
extern "C" 
__attribute__((noinline)) 
void ft_sched_entrypoint()
#else
int main()
#endif
{
	entrypoint();
}

#include "base.cpp"
#include "ft_sched.cpp"

