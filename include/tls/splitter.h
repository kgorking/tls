#ifndef TLS_SPLITTER_H
#define TLS_SPLITTER_H

#include <mutex>
#include <vector>

namespace tls {
// Provides a thread-local instance of the type T for each thread that
// accesses it. This avoid having to use locks to read/write data.
// This class only locks when a new thread is created/destroyed.
// The set of instances can be accessed through the begin()/end() iterators.
// Note: Two splitter<T> instances in the same thread will point to the same data.
//       Differentiate between them by passing different types to 'UnusedDifferentiaterType'.
//       As the name implies, it's not used internally, so just put whatever.
template <typename T, typename UnusedDifferentiaterType = struct o_O>
class splitter {
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
		T &get(splitter<T, UnusedDifferentiaterType> *instance) noexcept {
			// If the owner is null, (re-)initialize the instance.
			// Data may still be present if the thread_local instance is still active
			if (owner == nullptr) {
				data = {};
				owner = instance;
				instance->init_thread(this);
			}
			return data;
		}

		void remove(splitter<T, UnusedDifferentiaterType> *instance) noexcept {
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
		splitter<T, UnusedDifferentiaterType> *owner{};
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
		data.push_back(std::move(*t->get_data()));

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
	splitter() noexcept = default;
	splitter(splitter const &) = delete;
	splitter(splitter &&) noexcept = default;
	splitter &operator=(splitter const &) = delete;
	splitter &operator=(splitter &&) noexcept = default;
	~splitter() noexcept {
		clear();
	}

	// Get the thread-local instance of T
	T &local() noexcept {
		thread_local instance_access var{};
		return var.get(this);
	}

	// Collects all the threads data and returns it. This clears the stored data.
	std::vector<T> collect() noexcept {
		std::scoped_lock sl(mtx_storage);

		for (instance_access *instance = head; instance != nullptr; instance = instance->get_next()) {
			data.push_back(std::move(*instance->get_data()));
		}

		return std::move(data);
	}

	// Performa an action on all each instance of the data
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

#endif // !TLS_SPLITTER_H
