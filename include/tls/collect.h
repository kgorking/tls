#ifndef TLS_COLLECT_H
#define TLS_COLLECT_H

#include <mutex>
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
		constexpr ~thread_data() noexcept {
			if (owner != nullptr) {
				owner->remove_thread(this);
			}
		}

		// Return a reference to an instances local data
		[[nodiscard]] constexpr T& get(collect* instance) noexcept {
			// If the owner is null, (re-)initialize the thread.
			// Data may still be present if the thread_local instance is still active
			if (owner == nullptr) {
				data = {};
				owner = instance;
				instance->init_thread(this);
			}
			return data;
		}

		constexpr void remove(collect* instance) noexcept {
			if (owner == instance) {
				data = {};
				owner = nullptr;
				next = nullptr;
			}
		}

		[[nodiscard]] constexpr T* get_data() noexcept {
			return &data;
		}
		[[nodiscard]] constexpr T const* get_data() const noexcept {
			return &data;
		}

		constexpr void set_next(thread_data* ia) noexcept {
			next = ia;
		}
		[[nodiscard]] constexpr thread_data* get_next() noexcept {
			return next;
		}
		[[nodiscard]] constexpr thread_data const* get_next() const noexcept {
			return next;
		}

	private:
		T data{};
		collect* owner{};
		thread_data* next = nullptr;
	};

private:
	// the head of the threads that access this collect instance
	thread_data* head{};

	// All the data collected from threads
	std::vector<T> data{};

	// Mutex for serializing access for adding/removing thread-local instances
	std::shared_mutex mtx;

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
		data.push_back(std::move(*local_data));

		// Reset the thread data
		*local_data = T{};

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

		return std::move(data);
	}

	// Gathers all the threads data and sends it to the output iterator. This clears all stored data.
	void gather_flattened(auto dest_iterator) noexcept {
		std::scoped_lock sl(mtx);
		for (T& t : data) {
			std::move(t.begin(), t.end(), dest_iterator);
		}
		data.clear();

		for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
			T* ptr_t = thread->get_data();
			std::move(ptr_t->begin(), ptr_t->end(), dest_iterator);
			*ptr_t = T{};
		}
	}

	// Perform an action on all threads data
	template <class Fn>
	void for_each(Fn&& fn) noexcept {
		std::scoped_lock sl(mtx);
		for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
			fn(*thread->get_data());
		}

		std::for_each(data.begin(), data.end(), std::forward<Fn>(fn));
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
