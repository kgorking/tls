#ifndef TLS_COLLECT_H
#define TLS_COLLECT_H

#include <mutex> // for std::scoped_lock
#include <shared_mutex>
#include <vector>

namespace tls {
// Works like tls::split, except data is preserved when threads die.
// You can collect the thread_local T's with collect::gather,
// which also resets the data on the threads by moving it.
// Note: Two runtime collect<T> instances in the same thread will point to the same data.
//       If they are evaluated in a constant context each instance has their own data.
//       Differentiate between runtime versions by passing different types to 'UnusedDifferentiaterType',
// 	     so fx collect<int, struct A> is different from collect<int, struct B>.
//       As the name implies, 'UnusedDifferentiaterType' is not used internally, so just put whatever.
template <typename T, typename UnusedDifferentiaterType = void>
class collect {
	// This struct manages the instances that access the thread-local data.
	// Its lifetime is marked as thread_local, which means that it can live longer than
	// the splitter<> instance that spawned it.
	struct thread_data {
		~thread_data() noexcept {
			if (owner != nullptr) {
				owner->remove_thread(this);
			}
		}

		// Return a reference to an instances local data
		[[nodiscard]] T& get(collect* instance) noexcept {
			// If the owner is null, (re-)initialize the thread.
			// Data may still be present if the thread_local instance is still active
			if (owner == nullptr) {
				data = {};
				owner = instance;
				instance->init_thread(this);
			}
			return data;
		}

		void remove(collect* instance) noexcept {
			if (owner == instance) {
				data = {};
				owner = nullptr;
				next = nullptr;
			}
		}

		[[nodiscard]] T* get_data() noexcept {
			return &data;
		}
		[[nodiscard]] T const* get_data() const noexcept {
			return &data;
		}

		void set_next(thread_data* ia) noexcept {
			next = ia;
		}
		[[nodiscard]] thread_data* get_next() noexcept {
			return next;
		}
		[[nodiscard]] thread_data const* get_next() const noexcept {
			return next;
		}

	private:
		T data{};
		collect* owner{};
		thread_data* next = nullptr;
	};

private:
	// the head of the threads that access this collect instance
	inline static thread_data* head{};

	// All the data collected from threads
	inline static std::vector<T> data{};

	// Mutex for serializing access for adding/removing thread-local instances
	inline static std::shared_mutex mtx;

	// Adds a new thread
	void init_thread(thread_data* t) noexcept {
		std::scoped_lock sl(mtx);
		t->set_next(head);
		head = t;
	}

	// Removes the thread
	void remove_thread(thread_data* t) noexcept {
		std::scoped_lock sl(mtx);

		// Take the thread data
		T* local_data = t->get_data();
		data.push_back(static_cast<T&&>(*local_data));

		// Reset the thread data
		*local_data = T{};

		// Remove the thread from the linked list
		if (head == t) {
			head = t->get_next();
		} else {
			auto curr = head;
			if (nullptr != curr) {
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
	}

public:
	collect() noexcept = default;
	collect(collect const&) = delete;
	collect(collect&&) noexcept = default;
	collect& operator=(collect const&) = delete;
	collect& operator=(collect&&) noexcept = default;
	~collect() noexcept {
		reset();
	}

	// Get the thread-local thread of T
	[[nodiscard]] T& local() noexcept {
		thread_local thread_data var{};
		return var.get(this);
	}

	// Gathers all the threads data and returns it. This clears all stored data.
	[[nodiscard]] std::vector<T> gather() noexcept {
		std::scoped_lock sl(mtx);
		for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
			data.push_back(std::move(*thread->get_data()));
			*thread->get_data() = T{};
		}

		return static_cast<std::vector<T>&&>(data);
	}

	// Gathers all the threads data and sends it to the output iterator. This clears all stored data.
	void gather_flattened(auto dest_iterator) noexcept {
		std::scoped_lock sl(mtx);
		using U = typename T::value_type;

		for (T& per_thread_data : data) {
			//std::move(t.begin(), t.end(), dest_iterator);
			for (U& elem : per_thread_data) {
				*dest_iterator = static_cast<U&&>(elem);
				++dest_iterator;
			}
		}
		data.clear();

		for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
			T* ptr_per_thread_data = thread->get_data();
			//std::move(ptr_per_thread_data->begin(), ptr_per_thread_data->end(), dest_iterator);
			for (U& elem : *ptr_per_thread_data) {
				*dest_iterator = static_cast<U&&>(elem);
				++dest_iterator;
			}
			//*ptr_per_thread_data = T{};
			ptr_per_thread_data->clear();
		}
	}

	// Perform an action on all threads data
	template <class Fn>
	void for_each(Fn&& fn) noexcept {
		std::scoped_lock sl(mtx);
		for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
			fn(*thread->get_data());
		}

		for (auto& d : data)
			fn(d);
	}

	// Perform an non-modifying action on all threads data
	template <class Fn>
	void for_each(Fn&& fn) const noexcept {
		{
			std::scoped_lock sl(mtx);
			for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
				fn(*thread->get_data());
			}
		}

		for (auto const& d : data)
			fn(d);
	}

	// Clears all data
	void clear() noexcept {
		std::scoped_lock sl(mtx);
		for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
			*(thread->get_data()) = {};
		}

		data.clear();
	}

	// Resets all data and threads
	void reset() noexcept {
		std::scoped_lock sl(mtx);
		for (thread_data* thread = head; thread != nullptr;) {
			auto next = thread->get_next();
			thread->remove(this);
			thread = next;
		}

		head = nullptr;
		data.clear();
	}
};
} // namespace tls

#endif // !TLS_COLLECT_H
