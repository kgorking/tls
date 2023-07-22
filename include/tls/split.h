#ifndef TLS_SPLIT_H
#define TLS_SPLIT_H

#include <mutex>
#include <shared_mutex>
#include <concepts>

namespace tls {
// Provides a thread-local instance of the type T for each thread that
// accesses it. Data is not preserved when threads die.
// This class locks when a thread is created/destroyed.
// The thread_local T's can be accessed through split::for_each.
// Use `tls::unique_split` or pass different types to 'UnusedDifferentiatorType'
// to create different types.
template <typename T, typename UnusedDifferentiaterType = void>
class split {
	// This struct manages the instances that access the thread-local data.
	// Its lifetime is marked as thread_local, which means that it can live longer than
	// the split<> instance that spawned it.
	struct thread_data {
		thread_data() {
			split::init_thread(this);
		}

		~thread_data() {
			split::remove_thread(this);
		}

		// Return a reference to an instances local data
		[[nodiscard]] T& get() noexcept {
			return data;
		}

		void remove() noexcept {
			data = {};
			next = nullptr;
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
		thread_data* next = nullptr;
	};

private:
	// the head of the threads
	inline static thread_data* head{};

	// Mutex for serializing access for adding/removing thread-local instances
	inline static std::shared_mutex mtx;

protected:
	// Adds a thread_data
	static void init_thread(thread_data* t) {
		std::unique_lock sl(mtx);
		t->set_next(head);
		head = t;
	}

	// Remove the thread_data
	static void remove_thread(thread_data* t) {
		std::unique_lock sl(mtx);
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
	// Get the thread-local instance of T
	T& local() noexcept {
		thread_local thread_data var{};
		return var.get();
	}

	// Performa an action on all each instance of the data
	template <class Fn>
	static void for_each(Fn&& fn) {
		if constexpr (std::invocable<Fn, T const&>) {
			std::shared_lock sl(mtx);
			for (thread_data const* thread = head; thread != nullptr; thread = thread->get_next()) {
				fn(*thread->get_data());
			}
		} else {
			std::unique_lock sl(mtx);
			for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
				fn(*thread->get_data());
			}
		}
	}

	// Clears all data
	static void clear() {
		std::unique_lock sl(mtx);
		for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
			*(thread->get_data()) = {};
		}
	}
};

template <typename T, typename U = decltype([] {})>
using unique_split = split<T, U>;

} // namespace tls

#endif // !TLS_SPLIT_H
