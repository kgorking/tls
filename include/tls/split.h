#ifndef TLS_SPLIT_H
#define TLS_SPLIT_H

#include <mutex>

namespace tls {
// Provides a thread-local instance of the type T for each thread that
// accesses it. Data is not preserved when threads die.
// This class locks when a thread is created/destroyed.
// The thread_local T's can be accessed through split::for_each.
// Note: Two split<T> instances in the same thread will point to the same data.
//       Differentiate between them by passing different types to 'UnusedDifferentiaterType'.
//       As the name implies, it's not used internally, so just put whatever.
template <typename T, typename UnusedDifferentiaterType = void>
class split {
	// This struct manages the instances that access the thread-local data.
	// Its lifetime is marked as thread_local, which means that it can live longer than
	// the split<> instance that spawned it.
	struct instance_access {
		// The destructor triggers when a thread dies and the thread_local
		// instance is destroyed
		~instance_access() noexcept {
			if (owner != nullptr) {
				owner->remove_thread(this);
			}
		}

		// Return a reference to an instances local data
		T &get(split *instance) noexcept {
			// If the owner is null, (re-)initialize the instance.
			// Data may still be present if the thread_local instance is still active
			if (owner == nullptr) {
				data = {};
				owner = instance;
				instance->init_thread(this);
			}
			return data;
		}

		void remove(split *instance) noexcept {
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
		split<T, UnusedDifferentiaterType> *owner{};
		instance_access *next = nullptr;
	};
	friend instance_access;

private:
	// the head of the threads that access this split instance
	instance_access *head{};

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
	split() noexcept = default;
	split(split const &) = delete;
	split(split &&) noexcept = default;
	split &operator=(split const &) = delete;
	split &operator=(split &&) noexcept = default;
	~split() noexcept {
		clear();
	}

	// Get the thread-local instance of T
	T &local() noexcept {
		thread_local instance_access var{};
		return var.get(this);
	}

	// Performa an action on all each instance of the data
	template<class Fn>
	void for_each(Fn&& fn) {
		std::scoped_lock sl(mtx_storage);

		for (instance_access *instance = head; instance != nullptr; instance = instance->get_next()) {
			fn(*instance->get_data());
		}
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
	}
};
} // namespace tls

#endif // !TLS_SPLIT_H
