#include "ft_sched.hpp"

[[nodiscard]]
bool DeadlineWatcher::watch(RawTask* task, Duration limit){
	auto guard = _lock.guard();

	DeadlineSlot* free_slot = nullptr;

	for(auto& slot : slots){
		if(slot.task == nullptr){
			free_slot = &slot;
		}
	}

	if(!free_slot){
		return false;
	}
	
	free_slot->task = task;
	free_slot->limit = limit;
	free_slot->last_tick = tick_now();
	task->deadline = free_slot;
	_count.fetch_add(1, memory_order_relaxed);

	return true;
}

void DeadlineWatcher::_remove_no_lock(DeadlineSlot* node){
	if(!node){ return; }
	ensure(node >= slots.data && node < (slots.data + slots.len), "invalid slot");

	node->task = nullptr;
	node->limit = {0};
	_count.fetch_add(-1, memory_order_relaxed);
}

void DeadlineWatcher::remove(DeadlineSlot* node){
	auto g = _lock.guard();
	_remove_no_lock(node);
}

void DeadlineWatcher::clear(){
	auto guard = _lock.guard();
	mem_zero(slots.data, slots.len * sizeof(*slots.data));
}

extern "C" int printf(cstring, ...);

// Scan for deadline violations and remove Done tasks
bool DeadlineWatcher::scan(){
	auto guard = _lock.guard();

	auto now = tick_now();
	bool ok = true;

	for(auto& slot : slots){
		if(!slot.task){
			continue;
	 	}

	 	auto status = slot.task->status();
		if(status >= TaskStatus_Done){
			_remove_no_lock(&slot);
			continue;
		}

		auto elapsed = tick_diff(now, slot.last_tick);

		// printf("CHECKING: %d NOW=%td LAST=%td E=%td MAX=%td\n", slot.task->id,
		// 	tick_diff(now, 0).to_milli(), tick_diff(slot.last_tick, 0).to_milli(),
		// 	elapsed.to_milli(), slot.limit.to_milli()
		// ); fflush(stdout);

		if(elapsed > slot.limit){
			printf("DEADLINE VIOLATION ON TASK %d\n", int(slot.task->id));
			slot.task->cancel();
			ok = false;
		}
	}

	return ok;
}

void init_deadline_watcher(DeadlineWatcher* w, Slice<DeadlineSlot> slots){
	mem_zero(w, sizeof(DeadlineWatcher));
	w->slots = slots;
	w->clear();
}

DeadlineWatcher* make_deadline_watcher(Arena* a, usize slot_count){
	auto restore = a->offset;
	auto watcher = make<DeadlineWatcher>(a);
	auto slots = make_slice<DeadlineSlot>(a, slot_count);

	if(!watcher || !slots){
		a->offset = restore;
		return nullptr;
	}

	watcher->slots = slots;
	watcher->clear();
	return watcher;
}

