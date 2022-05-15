#ifndef TLS_SPLIT_H
#define TLS_SPLIT_H

#include <mutex>
#include <shared_mutex>

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
	struct thread_data {
		// The destructor triggers when a thread dies and the thread_local
		// instance is destroyed
		constexpr ~thread_data() noexcept {
			if (owner != nullptr) {
				owner->remove_thread(this);
			}
		}

		// Return a reference to an instances local data
		[[nodiscard]] constexpr T& get(split* instance) noexcept {
			// If the owner is null, (re-)initialize the instance.
			// Data may still be present if the thread_local instance is still active
			if (owner == nullptr) {
				data = {};
				owner = instance;
				instance->init_thread(this);
			}
			return data;
		}

		constexpr void remove(split* instance) noexcept {
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
		split<T, UnusedDifferentiaterType> *owner{};
		thread_data *next = nullptr;
	};

private:
	// the head of the threads that access this split instance
	thread_data *head{};

	// Mutex for serializing access for adding/removing thread-local instances
	std::shared_mutex* mtx_storage{};

	// Data that is only used in constexpr evaluations
	thread_data consteval_data{};

protected:
	[[nodiscard]] std::shared_mutex& get_runtime_mutex() noexcept {
		return *mtx_storage;
	}

	// Adds a thread_data
	constexpr void init_thread(thread_data* t) noexcept {
		auto const init_thread_imp = [&]() noexcept {
			t->set_next(head);
			head = t;
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			init_thread_imp();
		} else {
			init_thread_imp();
		}
	}

	// Remove the thread_data
	constexpr void remove_thread(thread_data* t) noexcept {
		auto const remove_thread_impl = [&]() noexcept {
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
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			remove_thread_impl();
		} else {
			remove_thread_impl();
		}
	}

public:
	constexpr split() noexcept {
		if (!std::is_constant_evaluated())
			mtx_storage = new std::shared_mutex;
	}
	constexpr split(split const&) = delete;
	constexpr split(split&&) noexcept = default;
	constexpr split& operator=(split const&) = delete;
	constexpr split& operator=(split&&) noexcept = default;
	constexpr ~split() noexcept {
		reset();

		if (!std::is_constant_evaluated())
			delete mtx_storage;
	}

	// Get the thread-local instance of T
	constexpr T& local() noexcept {
		if (!std::is_constant_evaluated()) {
			auto const local_impl = [&]() -> T& {
				thread_local thread_data var{};
				return var.get(this);
			};
			return local_impl();
		} else {
			return consteval_data.get(this);
		}
	}

	// Performa an action on all each instance of the data
	template<class Fn>
	constexpr void for_each(Fn&& fn) {
		auto const for_each_impl = [&]() noexcept {
			for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
				fn(*thread->get_data());
			}
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			for_each_impl();
		} else {
			for_each_impl();
		}
	}

	// Resets all data and threads
	constexpr void reset() noexcept {
		auto const impl = [&] {
			for (thread_data* instance = head; instance != nullptr;) {
				auto next = instance->get_next();
				instance->remove(this);
				instance = next;
			}

			head = nullptr;
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			impl();
		} else {
			impl();
		}
	}
};
} // namespace tls

#endif // !TLS_SPLIT_H
