// IMPORTANT: The tasks will be concurrently executed, it the caller's
// responsibility to ensure that whatever arguments were read, either by
// capture or explicitly are thread safe and do not interfere.
template<typename Output, Callable<Output, TaskContext> TaskFunc, Callable<void, TaskContext> OnCancel>
struct TMR_Task {
	struct SubTask {
		RawTask task;
		TaskFunc func;
		Option<Output> result;
	};

	RawTask supervisor;
	DeadlineWatcher watcher;
	Array<SubTask, 3> workers;
	Duration worker_deadline;
	ConsensusFunc<Option<Output>> consensus_func;

	static void _worker_wrapper(RawTask* t){
		auto sub_task = (SubTask*)t->args;
		auto ctx = TaskContext { &sub_task->task };
		sub_task->result = sub_task->func(ctx);
	}

	static void _supervisor_wrapper(RawTask* t){
		auto self = (TMR_Task<Output, TaskFunc, OnCancel>*)t->args;

		for(int i = 0; i < 3; i ++){
			self->workers[i].task.run();
		}

		while(self->watcher.count()){
			self->watcher.scan();
			sleep_for(Duration::from_milli(1));
		}
	}

	void run(){
		supervisor.run();
	}

	void join(){
		supervisor.join();
	}

	Option<Output> result(){
		if(supervisor.status() < TaskStatus_Done){
			return {};
		}

		int n = consensus(workers[0].result, workers[1].result, workers[2].result, consensus_func);
		if(n < 0){
			return {};
		}

		return move(workers[n].result);
	}

	void cancel(){
		for(int i = 0; i < 3; i++){
			workers.task[i].cancel();
		}
		supervisor.cancel();
	}

	TaskStatus status(){
		return supervisor._status.load(memory_order_relaxed);
	}

	RawTask* raw_task(){
		return &supervisor;
	}

	u32 id(){
		return supervisor.id;
	}

	TMR_Task()
		: supervisor{}
		, watcher{}
		, workers{}
		, worker_deadline{0}
		, consensus_func{default_consensus_func<Option<Output>>}
	{}
};

static_assert(Task<TMR_Task<Unit, decltype(_task_nop), decltype(_cancellation_nop)>, Unit>, "BasicTask does not conform to Task concept");

template<typename F>
auto make_tmr_task(Arena* arena, Duration worker_deadline, F&& func){
	auto t = make<TMR_Task<decltype(func(TaskContext{})), F, decltype(_cancellation_nop)>>(arena);
	auto slots = make_slice<DeadlineSlot>(arena, 3);
	ensure(t, "Failed to create TMR task");

	init_deadline_watcher(&t->watcher, slots);
	t->worker_deadline = worker_deadline;
	init_raw_task(&t->supervisor, arena, t->_supervisor_wrapper, t);

	for(int i = 0; i < 3; i++){
		t->workers[i].func = func;
		init_raw_task(&t->workers[i].task, arena, t->_worker_wrapper, &t->workers[i]);

		auto res = t->watcher.watch(&t->workers[i].task, t->worker_deadline);
		ensure(res, "Failed to watch");
	}

	return t;
}
