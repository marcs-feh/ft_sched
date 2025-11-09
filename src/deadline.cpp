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

	ensure(task->supervisor == nullptr || task->supervisor == this, "This task already has a supervisor attached");
	task->supervisor = this;

	_count.fetch_add(1, memory_order_relaxed);

	return true;
}

DeadlineSlot* DeadlineWatcher::get(RawTask* t){
	auto g = _lock.guard();
	for(auto& slot : slots){
		if(slot.task == t){
			return &slot;
		}
	}
	return nullptr;
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

bool DeadlineWatcher::reset_deadline(RawTask* t){
	auto guard = _lock.guard();
	for(auto& slot : slots){
		if(slot.task == t){
			slot.reset();
			return true;
		}
	}
	return false;
}

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

		if(elapsed > slot.limit){
			printf("[Error] Deadline Violation, cancelling task (%d)\r\n", int(slot.task->id));
			slot.task->cancel();
			_remove_no_lock(&slot);
			ok = false;
		}
	}

	return ok;
}

RawTask* DeadlineWatcher::scan_until_violation(){
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

		if(elapsed > slot.limit){
			printf("[Error] Deadline Violation on task (%d)\r\n", int(slot.task->id));
			// slot.task->cancel();
			// _remove_no_lock(&slot);
			return slot.task;
		}
	}

	return nullptr;
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

