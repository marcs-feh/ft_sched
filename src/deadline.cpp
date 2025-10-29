#include "ft_sched.hpp"

[[nodiscard]]
DeadlineSlot* DeadlineWatcher::add(RawTask* task, Duration limit){
	auto guard = _lock.guard();

	DeadlineSlot* free_slot = nullptr;

	for(auto& slot : slots){
		if(slot.task == nullptr){
			free_slot = &slot;
		}
	}

	if(free_slot){
		free_slot->task = task;
		free_slot->limit = limit;
	}

	return free_slot;
}

void DeadlineWatcher::_remove_no_lock(DeadlineSlot* node){
	if(!node){ return; }
	ensure(node >= slots.data && node < (slots.data + slots.len), "invalid slot");

	node->task = nullptr;
	node->limit = {0};
}

void DeadlineWatcher::remove(DeadlineSlot* node){
	auto g = _lock.guard();
	_remove_no_lock(node);
}

void DeadlineWatcher::clear(){
	auto guard = _lock.guard();
	mem_zero(slots.data, slots.len * sizeof(*slots.data));
}

	// Scan for deadline violations and remove Done tasks
void DeadlineWatcher::scan(){
	auto guard = _lock.guard();

	auto now = tick_now();
	for(auto& slot : slots){
		if(slot.task->status() == TaskStatus_Done){
			_remove_no_lock(&slot);
			continue;
		}

		if(tick_diff(now, slot.last_tick) > slot.limit){
			u8 buf[80];
			auto msg =buffer_printf(Slice<u8>{&buf[0], sizeof(buf)}, "expired deadline on task %p", slot.task);
			panic(msg.data);
		}
	}
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

