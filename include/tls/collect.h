#ifndef TLS_COLLECT_H
#define TLS_COLLECT_H

#include <mutex>
#include <vector>

namespace tls {
// Works like tls::split, except data is preserved when threads die.
// You can collect the thread_local T's with collect::gather,
// which also resets the data on the threads by moving it.
// Note: Two collect<T> instances in the same thread will point to the same data.
//       Differentiate between them by passing different types to 'UnusedDifferentiaterType'.
//       As the name implies, it's not used internally, so just put whatever.
template <typename T, typename UnusedDifferentiaterType = void>
class collect {
	// This struct manages the instances that access the thread-local data.
	// Its lifetime is marked as thread_local, which means that it can live longer than
	// the splitter<> instance that spawned it.
	struct instance_access {
		~instance_access() noexcept {
			if (owner != nullptr) {
				owner->remove_thread(this);
			}
		}

		// Return a reference to an instances local data
		T &get(collect *instance) noexcept {
			// If the owner is null, (re-)initialize the instance.
			// Data may still be present if the thread_local instance is still active
			if (owner == nullptr) {
				data = {};
				owner = instance;
				instance->init_thread(this);
			}
			return data;
		}

		void remove(collect *instance) noexcept {
			if (owner == instance) {
				data = {};
				owner = nullptr;
				next = nullptr;
			}
		}

		T *get_data() noexcept {
			return &data;
		}
		T const *get_data() const noexcept {
			return &data;
		}

		void set_next(instance_access *ia) noexcept {
			next = ia;
		}
		instance_access *get_next() noexcept {
			return next;
		}
		instance_access const *get_next() const noexcept {
			return next;
		}

	private:
		T data{};
		collect *owner{};
		instance_access *next = nullptr;
	};
	friend instance_access;

private:
	// the head of the threads that access this splitter instance
	instance_access *head{};

	// All the data collected from threads
	std::vector<T> data;

	// Mutex for serializing access for adding/removing thread-local instances
	std::mutex mtx_storage;

protected:
	// Adds a instance_access
	void init_thread(instance_access *t) noexcept {
		std::scoped_lock sl(mtx_storage);

		t->set_next(head);
		head = t;
	}

	// Remove the instance_access
	void remove_thread(instance_access *t) noexcept {
		std::scoped_lock sl(mtx_storage);

		// Take the thread data
		T *thread_data = t->get_data();
		data.push_back(std::move(*thread_data));

		// Reset the thread data
		*thread_data = T{};

		// Remove the thread from the linked list
		if (head == t) {
			head = t->get_next();
		} else {
			auto curr = head;
			while (curr->get_next() != nullptr) {
				if (curr->get_next() == t) {
					curr->set_next(t->get_next());
					return;
				} else {
					curr = curr->get_next();
				}
			}
		}
	}

public:
	collect() noexcept = default;
	collect(collect const &) = delete;
	collect(collect &&) noexcept = default;
	collect &operator=(collect const &) = delete;
	collect &operator=(collect &&) noexcept = default;
	~collect() noexcept {
		clear();
	}

	// Get the thread-local instance of T
	T &local() noexcept {
		thread_local instance_access var{};
		return var.get(this);
	}

	// Gethers all the threads data and returns it. This clears all stored data.
	std::vector<T> gather() noexcept {
		std::scoped_lock sl(mtx_storage);

		for (instance_access *instance = head; instance != nullptr; instance = instance->get_next()) {
			data.push_back(std::move(*instance->get_data()));
		}

		return std::move(data);
	}

	// Perform an action on all each instance of the data
	template<class Fn>
	void for_each(Fn&& fn) {
		std::scoped_lock sl(mtx_storage);

		for (instance_access *instance = head; instance != nullptr; instance = instance->get_next()) {
			fn(*instance->get_data());
		}

		std::for_each(data.begin(), data.end(), fn);
	}

	// Clears all data and threads
	void clear() noexcept {
		std::scoped_lock sl(mtx_storage);

		for (instance_access *instance = head; instance != nullptr;) {
			auto next = instance->get_next();
			instance->remove(this);
			instance = next;
		}

		head = nullptr;
		data.clear();
	}
};
} // namespace tls

#endif // !TLS_COLLECT_H
