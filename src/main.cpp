#include <stdio.h>
#include <math.h>

#if defined(FT_SCHED_PLATFORM_STM32F411CEU6)
#include "FreeRTOS.h"
#include "semphr.h"
#endif

// #define FT_USE_CRC32

#include "base.hpp"

#include "ft_sched.hpp"

#include "image.cpp"

#include "lena.pgm.cpp"

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

constexpr usize task_arena_size = 24 * 1024;
alignas(64) u8 task_arena_memory[task_arena_size];

constexpr usize main_arena_size = 8000;
alignas(64) u8 main_arena_memory[main_arena_size];

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

i32 sobel_row(Bitmap const& input, i32 row, Slice<u8> output, Arena* scratch){
	ensure(input.width == output.len, "Mismatched width");
	Convolution_Context<3> conv_horiz{};

	auto region = arena_region_begin(scratch);
	defer(arena_region_end(region));

	conv_horiz.input = input;
	conv_horiz.scratch = scratch;
	conv_horiz.use_kernel(Array<i32, 9>{
		-1000, -2000, -1000,
		    0,     0,     0,
		+1000, +2000, +1000,
	} / splat<i32, 9>(4));

	Convolution_Context<3> conv_vert = conv_horiz;
	conv_horiz.scratch = scratch;
	conv_vert.use_kernel(Array<i32, 9>{
		-1000, 0, +1000,
		-2000, 0, +2000,
		-1000, 0, +1000,
	} / splat<i32, 9>(4));

	i32 ok_count = 0;
	for(i32 x = 0; x < input.width; x++){
		i32 res = conv_horiz.get(x, row) + conv_vert.get(x, row);
		if(res >= 0){
			ok_count++;
		}
		output[x] = u8(clamp<i32>(0, res, 255));
	}

	return ok_count;
}

static u8 image_output_storage[sizeof(image_pgm_data_storage)];
static auto image_output_data = Slice<u8>{&image_output_storage[0], sizeof(image_output_storage)};

template<typename F>
static inline
Duration time_it(F f){
	auto begin = tick_now();
	f();
	auto end = tick_now();
	return tick_diff(end, begin);
}

i32 consensus(Slice<u8> a, Slice<u8> b, Slice<u8> c){
	bool ab = a == b;
	bool bc = b == c;
	bool ca = c == a;

	// printf("AB: %d | BC: %d | CA: %d\r\n", int(ab), int(bc), int(ca));

	if(ab) return 0;
	if(bc) return 1;
	if(ca) return 2;

	return -1;
}

#define println(FMT, ...) do {\
	printf(FMT "\r\n" __VA_OPT__(,) __VA_ARGS__); \
	sleep_for(Duration::from_milli(2)); \
} while(0)



attribute_force_inline
static inline Pair<int> do_sobel_simple(Bitmap const& image, Bitmap& output){
	auto begin = tick_now();
	auto row_arena = main_arena.make_sub(350);
	Duration line_time_acc = {0};

	for(i32 row = 0; row < image.height; row += 1){
		auto output_row = make_slice<u8>(row_arena, image.width);
		i32 ok = 0;

		Duration line_time = time_it([&](){
			ok = sobel_row(image, row, output_row, row_arena);
		});
		line_time_acc = line_time_acc + line_time;

		if(ok != image.width){
			printf("Row error\r\n");
		}
		auto dest = output.pixel_data.skip(row * output.width).take(output.width);
		copy(dest, output_row);
		row_arena->offset = 0;
	}

	auto end = tick_now();
	auto elapsed = tick_diff(end, begin);

	return {int(elapsed.to_milli()), int(line_time_acc.to_milli() / image.height)};
}

attribute_force_inline
static inline Pair<int> do_sobel_reexec(Bitmap const& image, Bitmap& output){
	auto begin = tick_now();
	auto row_arena = main_arena.make_sub(350 * 3);
	Duration line_time_acc = {0};
	Slice<u8> output_rows[3];

    for(i32 row = 0; row < image.height; row += 1){
        output_rows[0] = make_slice<u8>(row_arena, image.width);
        output_rows[1] = make_slice<u8>(row_arena, image.width);
        output_rows[2] = make_slice<u8>(row_arena, image.width);

        ensure(output_rows[0].data && output_rows[1].data && output_rows[2].data, "Failed to allocate rows");

        Duration line_time = time_it([&](){
            sobel_row(image, row, output_rows[0], row_arena);
            sobel_row(image, row, output_rows[1], row_arena);
            sobel_row(image, row, output_rows[2], row_arena);
        });

        line_time_acc = line_time_acc + line_time;

        auto dest = output.pixel_data.skip(row * output.width).take(output.width);

        i32 cons = consensus(output_rows[0], output_rows[1], output_rows[2]);
        ensure(cons >= 0, "No consensus reached");

        if(output_rows[cons].len != image.width){
            panic("Row error\r\n");
        }
        copy(dest, output_rows[cons]);
        row_arena->offset = 0;
    }

	auto end = tick_now();
	auto elapsed = tick_diff(end, begin);

	return {int(elapsed.to_milli()), int(line_time_acc.to_milli() / image.height)};
}

attribute_force_inline
static inline Pair<int> do_sobel_tmr(Bitmap const& image, Bitmap& output){
	auto begin = tick_now();

	Arena* row_arenas[3];
	row_arenas[0] = main_arena.make_sub(350);
	row_arenas[1] = main_arena.make_sub(350);
	row_arenas[2] = main_arena.make_sub(350);

	ensure(row_arenas[0] && row_arenas[1] && row_arenas[2], "Failed to allocate row arenas");

	Duration line_time_acc = {0};
	Slice<u8> output_rows[3];

	output_rows[0] = make_slice<u8>(row_arenas[0], image.width);
	output_rows[1] = make_slice<u8>(row_arenas[1], image.width);
	output_rows[2] = make_slice<u8>(row_arenas[2], image.width);

	Atomic<bool> do_work = false;

	Duration tmr0_time = {0};
	i32 row = 0;

	static Atomic<i32> done_count = 0;

	SemaphoreHandle_t ready[3];
	ready[0] = xSemaphoreCreateBinary();
	ready[1] = xSemaphoreCreateBinary();
	ready[2] = xSemaphoreCreateBinary();

	TaskHandle_t coordinator = xTaskGetCurrentTaskHandle();

	auto tmr0 = make_basic_task(&task_arena, 0, [&](TaskContext ctx){
		while(1){
			xSemaphoreTake(ready[0], portMAX_DELAY);

			sobel_row(image, row, output_rows[0], row_arenas[0]);

			done_count.fetch_add(1);
		}

		panic("Unreachable");
		return Unit{};
	});

	auto tmr1 = make_basic_task(&task_arena, 0, [&](TaskContext ctx){
		while(1){
			xSemaphoreTake(ready[1], portMAX_DELAY);

			sobel_row(image, row, output_rows[1], row_arenas[1]);

			done_count.fetch_add(1);
		}

		panic("Unreachable");
		return Unit{};
	});

	auto tmr2 = make_basic_task(&task_arena, 0, [&](TaskContext ctx){
		while(1){
			xSemaphoreTake(ready[2], portMAX_DELAY);

			sobel_row(image, row, output_rows[2], row_arenas[2]);

			done_count.fetch_add(1);
		}

		panic("Unreachable");
		return Unit{};
	});

	for(row = 0; row < image.height; row += 1){
		ulTaskNotifyTake(pdTRUE, 0); // Clear pending

		// Notify workers
		xSemaphoreGive(ready[0]);
		xSemaphoreGive(ready[1]);
		xSemaphoreGive(ready[2]);

		while(done_count.load() != 3){
			task_yield();
		}
		println("Row: %d", int(row));

		done_count.store(0);
	}

	auto end = tick_now();
	auto elapsed = tick_diff(end, begin);
	return {int(elapsed.to_milli()), int(line_time_acc.to_milli() / image.height)};
}
#define FT_EXAMPLE_TMR

static inline
void entrypoint(){
	main_arena = arena_from_buffer({&main_arena_memory[0], main_arena_size});
	task_arena = arena_from_buffer({&task_arena_memory[0], task_arena_size});

	#if defined(FT_SCHED_PLATFORM_STM32F411CEU6)
		for(int i = 3; i > 0; i --){
			sleep_for(Duration::from_milli(1'000)); printf("%d\r\n", i); fflush(stdout);
		}
		print_info();
	#endif

	auto image = load_p5(image_pgm_data).unwrap();
	auto copy_time = time_it([=](){
		copy(image_output_data, image_pgm_data);
	});
	auto output = load_p5(image_output_data).unwrap();

	// printf("Loaded image: %dms\r\n", (int)copy_time.to_milli());

	ensure(output.pixel_data.len == image.pixel_data.len, "Mismatched output");

	println("[Sobel Filter]\r\n");

	#if defined(FT_EXAMPLE_SIMPLE)
	auto _ = make_basic_task(&task_arena, [](TaskContext){ return Unit{}; });

	auto [total_time, time_per_row] = do_sobel_simple(image, output);
	#elif defined(FT_EXAMPLE_REEXEC)
	auto _ = make_basic_task(&task_arena, [](TaskContext){ return Unit{}; });

	auto [total_time, time_per_row] = do_sobel_reexec(image, output);
	#elif defined(FT_EXAMPLE_TMR)
	auto [total_time, time_per_row] = do_sobel_tmr(image, output);
	#else
	#error "No example selected"
	#endif
	// auto begin = tick_now();
	// auto row_arena = main_arena.make_sub(350 * 3);
	//
	// Duration line_time_acc = {0};
	//
	// Slice<u8> output_rows[3];
	//
	// Atomic<bool> tmr0_running = false;
	// Duration tmr0_time = {0};
	// i32 row = 0;
	//
	// auto tmr0 = make_basic_task(&task_arena, 0, [&](TaskContext ctx){
	// 	while(1){
	// 		if(tmr0_running.load()){
	// 			// printf("TMR0 Begin\r\n");
	// 			tmr0_time = time_it([&](){
	// 				sobel_row(image, row, output_rows[0], row_arena);
	// 			});
	//
	// 			tmr0_running.store(false);
	// 			// printf("TMR0 End\r\n");
	// 		}
	//
	// 		sleep_for(Duration::from_milli(10));
	// 	}
	// 	return Unit{};
	// });
	//
	// for(row = 0; row < image.height; row += 1){
	// 	output_rows[0] = make_slice<u8>(row_arena, image.width);
	// 	// output_rows[1] = make_slice<u8>(row_arena, image.width);
	// 	// output_rows[2] = make_slice<u8>(row_arena, image.width);
	// 	// ensure(output_rows[0].data && output_rows[1].data && output_rows[2].data, "Failed to allocate rows");
	//
	// 	if(!tmr0_running.load()){
	// 		tmr0_running.store(true);
	// 	}
	//
	// 	while(tmr0_running.load()){
	// 		sleep_for(Duration::from_milli(10));
	// 	}
	//
	// 	line_time_acc = line_time_acc + tmr0_time;
	//
	// 	auto dest = output.pixel_data.skip(row * output.width).take(output.width);
	//
	// 	// i32 cons = consensus(output_rows[0], output_rows[1], output_rows[2]);
	// 	// ensure(cons >= 0, "No consensus reached");
	//
	// 	if(output_rows[0].len != image.width){
	// 		panic("Row error\r\n");
	// 	}
	// 	copy(dest, output_rows[0]);
	// 	row_arena->offset = 0;
	// }

	println("------------------");
	println("Took: %dms", total_time);
	println("ms/row: %dms", time_per_row);
	println("Task Arena: %dB", int(task_arena.peak_usage));
	println("Main Arena: %dB", int(main_arena.peak_usage));
	println("Image dimensions: %dx%d", int(image.width), int(image.height));
	println("------------------");
	fflush(stdout);

	#if !defined(FT_SCHED_PLATFORM_STM32F411CEU6)
	// dump_bitmap(output);
	#endif


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

