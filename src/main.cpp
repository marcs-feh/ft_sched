#include <stdio.h>
#include <math.h>

#include "base.hpp"

#include "ft_sched.hpp"

#include "image.cpp"

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

constexpr usize main_arena_size = 12 * 1024;
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

void do_regular_conv(){
	auto image = load_p5(image_pgm_data).unwrap();
	auto image_check_value = crc32(image);

	auto output = image.copy(&main_arena).unwrap();
	Convolution_Context<3> conv_horiz{};

	conv_horiz.input = image;
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

	auto begin = tick_now();
	for(i32 y = 0; y < image.height; y++){
		for(i32 x = 0; x < image.width; x++){
			output.pixel_data[(y * output.width) + x] = clamp<i32>(0, (conv_horiz.get(x, y) + conv_vert.get(x, y)), 255);
		}
		crc32_ensure(image_check_value, image);
	}

	auto end = tick_now();
	auto elapsed = tick_diff(end, begin);

	dump_bitmap(output);
}

template<typename Output, Callable<Output, TaskContext> TaskFunc, Callable<void, TaskContext> OnCancel>
struct TripleTask {
	struct SubTask : public BasicTask<Output, TaskFunc, OnCancel>{
		TripleTask* parent;

		SubTask(TaskFunc f, OnCancel c, TripleTask* p)
			: BasicTask<Output, TaskFunc, OnCancel>{f, c}
			, parent{p}
			{}
	};

	DeadlineWatcher* supervisor;
	DeadlineWatcher* subtasks_watcher;
	SubTask subtasks[3];
	Atomic<u32> running{0};

	void join(){
		while(subtasks_watcher->count()){
			auto scan = subtasks_watcher->scan();

			if(!scan){
				break;
			}

			sleep_for(Duration::from_milli(40));
		}

	}

	static void _subtask_wrapper(RawTask* t){
		auto subtask = (SubTask*)t->args;
		auto context = TaskContext { &subtask->_task };
		ensure(subtask->_task.supervisor == subtask->parent->subtasks_watcher && subtask->parent->subtasks_watcher, "Task not being monitored");

		subtask->parent->running.fetch_add(1, memory_order_relaxed);

		// Wait until siblings are initialized
		// while(subtask->parent->running.load(memory_order_relaxed) != 3){
		// 	task_yield();
		// }
		context.reset_deadline();

		subtask->_result = Output{ subtask->_func(context) };
		subtask->parent->running.fetch_sub(1, memory_order_relaxed);
		// subtask->parent->subtasks_watcher->remove_key(subtask);
	}


	static
	void _tmr_task_slot_cancellation(void* data){
		auto tmr = (TripleTask*)data;
		tmr->subtasks_watcher->clear();
		tmr->running.store(0);

		for(int i = 0; i < 3; i += 1){
			tmr->subtasks[i].cancel();
		}

	}

	[[nodiscard]]
	bool attach_supervisor(DeadlineWatcher* watcher, Duration limit){
		if(watcher == subtasks_watcher){
			panic("Recursive watcher is not allowed");
		}
		supervisor = watcher;
		return supervisor->add(this, _tmr_task_slot_cancellation, limit);
	}

	TripleTask(TaskFunc f)
		: supervisor{nullptr}
		, subtasks_watcher{nullptr}
		, subtasks{
			SubTask(f, _cancellation_nop, this),
			SubTask(f, _cancellation_nop, this),
			SubTask(f, _cancellation_nop, this)
		}
			
		, running{0}
	{}

};

template<typename F> [[nodiscard]]
auto make_tmr_task(
	Arena* arena,
	usize subtask_arena_size,
	usize subtask_stack_size,
	Duration subtask_deadline,
	F&& func
){
	auto restore = arena->offset;
	using TaskType = TripleTask<decltype(func(TaskContext{})), F, decltype(_cancellation_nop)>;

	auto tmr = make<TaskType>(arena, func);
	auto subtasks_watcher = make_deadline_watcher(arena, 6);
	if(!tmr || !subtasks_watcher){
		arena->offset = restore;
		return (TaskType*)nullptr;
	}

	tmr->running.store(0);
	tmr->subtasks_watcher = subtasks_watcher;

	for(int i = 0; i < 3; i++){
		auto subtask = &tmr->subtasks[i];

		auto watch_ok = subtask->raw_task()->attach_supervisor(tmr->subtasks_watcher, subtask_deadline);
		ensure(watch_ok, "Failed to watch task");

		if(!init_raw_task(subtask->raw_task(), arena, subtask_arena_size, subtask_stack_size, tmr->_subtask_wrapper, subtask)){
			arena->offset = restore;
			return (TaskType*)nullptr;
		}
	}

	return tmr;
}

static inline
void entrypoint(){
	main_arena = arena_from_buffer({&main_arena_memory[0], main_arena_size});
	task_arena = arena_from_buffer({&task_arena_memory[0], task_arena_size});

	auto image = load_p5(image_pgm_data).unwrap();
	bool running = true;

	auto convolve_horizontal_input = make_spsc_queue<i32>(&main_arena, 8);

	auto convolve_horizontal = [&running, convolve_horizontal_input, image](TaskContext ctx){
		i32 row_idx = 0;
		while(running){
			if(!convolve_horizontal_input->pop_into(&row_idx)){
				continue;
			}

			printf("RECV: %d\r\n", int(row_idx)); fflush(stdout);
			sleep_for(Duration::from_milli(10));
		}

		return row_idx;
	};

	#if defined(FT_SCHED_PLATFORM_STM32F411CEU6)
		for(int i = 4; i > 0; i --){
			sleep_for(Duration::from_milli(1'000)); printf("%d\r\n", i); fflush(stdout);
		}
		print_info();
	#endif
		swdg_init(Duration::from_milli(10'000));

		DeadlineWatcher* swdg_watcher = make_deadline_watcher(&task_arena, 32);
		mem_copy(&swdg_watcher->name, "SWDG", sizeof(swdg_watcher->name) - 1);

		ensure(swdg_watcher != nullptr, "Failed to create swdg_watcher");
		[[maybe_unused]] auto watcher_task = make_basic_task(&task_arena, 512, [swdg_watcher](TaskContext){
			while(1){
				// printf("[swdg] Scan %d\r\n", int(swdg_watcher->count())); fflush(stdout);
				auto ok = swdg_watcher->scan() && swdg_watcher->count();
				if(ok){
					swdg_reset();
				}

				sleep_for(Duration::from_milli(1));
			}

			return Unit{};
		});
	// #endif

	auto horiz_task = make_basic_task(&task_arena, 2000, 0, convolve_horizontal);
	horiz_task->attach_supervisor(swdg_watcher, Duration::from_milli(1'000));

	for(i32 i = 0; i < image.height; i += 1){
		convolve_horizontal_input->push(i);
	}

	horiz_task->join();

	// DeadlineWatcher* watcher = make_deadline_watcher(&task_arena, 32);
	// ensure(watcher, "Failed to create watcher");
	// auto tmr = make_tmr_task(&task_arena, 2048, 0, Duration::from_milli(1000), [](TaskContext ctx) -> Unit {
	// 	printf("Hello %d\r\n", int(ctx.id())); fflush(stdout);
	// 	sleep_for(Duration::from_milli(ctx.id() * 50));
	// 	printf("Bye %d\r\n", int(ctx.id())); fflush(stdout);
	// 	return {};
	// });

	// ensure(tmr, "Failed to create TMR task");

	// // bool attached = tmr->attach_supervisor(swdg_watcher, Duration::from_milli(4'000));
	// // ensure(attached, "Could not attach swdg supervisor");
	// tmr->join();

	// for(f32 x = 1.4; x < 100; x *= 2.00213){
	// 	Array<u8, 40> buf;
	// 	auto s = buffer_printf(buf.slice(), "%f", x);
	// 	printf("x = %.*s\r\n", int(s.len), cstring(s.data));
	// }

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

