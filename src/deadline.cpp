#include "ft_sched.hpp"

[[nodiscard]]
bool DeadlineWatcher::add(void* key, SlotCancellationCallback cancel, Duration limit){
	auto guard = _lock.guard();

	DeadlineSlot* free_slot = nullptr;

	for(auto& slot : slots){
		if(slot.key == nullptr){
			free_slot = &slot;
		}
	}

	if(!free_slot){
		return false;
	}
	
	free_slot->key = key;
	free_slot->limit = limit;
	free_slot->last_tick = tick_now();
	free_slot->cancel = cancel;

	_count.fetch_add(1);

	return true;
}

DeadlineSlot* DeadlineWatcher::get(void* key){
	auto g = _lock.guard();
	for(auto& slot : slots){
		if(slot.key == key){
			return &slot;
		}
	}
	return nullptr;
}

void DeadlineWatcher::_remove_no_lock(DeadlineSlot* node){
	if(!node){ return; }
	ensure(node >= slots.data && node < (slots.data + slots.len), "invalid slot");

	node->key = nullptr;
	node->cancel = nullptr;
	node->limit = {0};

	_count.fetch_sub(1);
}

void DeadlineWatcher::remove(DeadlineSlot* node){
	auto g = _lock.guard();
	_remove_no_lock(node);
}

void DeadlineWatcher::clear(){
	auto guard = _lock.guard();
	mem_zero(slots.data, slots.len * sizeof(*slots.data));
	_count.store(0);
}

extern "C" int printf(cstring, ...);

bool DeadlineWatcher::reset_deadline(void* key){
	auto guard = _lock.guard();
	for(auto& slot : slots){
		if(slot.key == key){
			slot.reset();
			return true;
		}
	}
	return false;
}

void DeadlineWatcher::remove_key(void* key){
	// auto guard = _lock.guard();

	for(auto& slot : slots){
		if(slot.key == key){
			_remove_no_lock(&slot);
		}
	}
}

// Scan for deadline violations and remove Done tasks
bool DeadlineWatcher::scan(){
	auto guard = _lock.guard();

	auto now = tick_now();
	bool ok = true;

	for(auto& slot : slots){
		if(!slot.key){
			continue;
	 	}

		auto elapsed = tick_diff(now, slot.last_tick);

		// printf("Scanning: %p limit=%d elapsed=%d\r\n", slot.key, int(slot.limit.to_milli()), int(elapsed.to_milli()));

		if(elapsed > slot.limit){
			if(name[0])
				printf("[Error Watcher=%s] Deadline Violation, Cancelling (%p)\r\n", name, slot.key);
			else
				printf("[Error Watcher=%p] Deadline Violation, Cancelling (%p)\r\n", this, slot.key);

			auto key = slot.key;
			auto cancel = slot.cancel;

			_remove_no_lock(&slot);
			ok = false;

			if(cancel){
				cancel(key);
			}
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

