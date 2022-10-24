#ifndef TLS_REPLICATE_H
#define TLS_REPLICATE_H

#include <shared_mutex>

namespace tls {
// This class can be used to replicate data from one writer thread to other reader threads.
// Data written to the replicator is copied to all threads that read from it, with minimal use of locking.
// The writer thread locks when data is written. Reader threads only lock when
// data is modified and needs to be updated.
// When data has not been modified it reads from its local copy.
template <class T, class UnusedDifferentiator = struct default_replicate_version>
class replicate {
	// This struct manages the per-thread data
	struct thread_data {
		// The thread is dead, so remove references to it from all instances
		~thread_data() {
			if (nullptr != replicator) {
				replicator->remove_thread(this);
			}
		}

		void remove(replicate *repl) noexcept {
			if (replicator == repl) {
				data_copy = {};
				replicator = nullptr;
				next = nullptr;
				invalidated = true;
			}
		}

		// Marks the data as out-of-date
		void invalidate() {
			invalidated = true;
		}

		// Returns the local data
		T const &read(replicate *repl) {
			maybe_update_data(repl);
			return data_copy;
		}

		// Pass the local data to a user function
		template <class Fn>
		auto read(replicate *repl, Fn&& f) {
			maybe_update_data(repl);

			// Only pass const-ref data to the function
			T const &const_data = data_copy;
			return f(const_data);
		}

		// Sets a copy of the data for an repl
		void set_data(T const &t) {
			data_copy = t;
		}

		// Resets the instances replicator to null
		void reset_owner() {
			replicator = nullptr;
		}

		void set_next(thread_data *ia) noexcept {
			next = ia;
		}
		thread_data *get_next() noexcept {
			return next;
		}
		thread_data const *get_next() const noexcept {
			return next;
		}

		// Check to see if the data needs updating
		void maybe_update_data(replicate *repl) {
			// Ensure this thread_data instance is initialized
			if (replicator == nullptr) {
				// this is the first time it has been accessed (ie. new thread)
				repl->init_thread(this);
				return;
			}

			// Update local data if the main data has changed
			if (invalidated) {
				invalidated = false;

				std::shared_lock sl(replicator->mtx);
				data_copy = replicator->data;
			}
		}

		T data_copy{};
		replicate<T> *replicator{nullptr};
		thread_data *next{nullptr};
		bool invalidated{true};
	};

	// Store a thread_data in this repl
	void init_thread(thread_data *t) {
		std::unique_lock ul(mtx);

		t->replicator = this;
		t->data_copy = data;
		t->invalidated = false;
		t->set_next(head);
		head = t;
	}

	// Removes a thread. Happens when they expire
	void remove_thread(thread_data *t) {
		std::unique_lock ul(mtx);

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

	// Get the thread_data repl for the current thread
	thread_data &local() {
		thread_local thread_data inst{};
		return inst;
	}

public:
	replicate(T &&t) : data(std::forward<T>(t)) {}
	replicate(T const &t) : data(t) {}

	~replicate() {
		std::unique_lock ul(mtx);
		for (auto *thread = head; thread != nullptr;) {
			auto next = thread->get_next();
			thread->remove(this);
			thread = next;
		}
	}

	// Read the threads copy of data
	T const &read() {
		return local().read(this);
	}

	// Pass the threads copy of data to a user function
	template <class Fn>
	auto read(Fn&& f) {
		return local().read(this, std::forward<Fn>(f));
	}

	// Set the master data
	void write(T const &t) {
		// Take a unique lock on the data so no other thread can modify it
		std::unique_lock ul(mtx);

		// Write the data
		data = t;

		// Notify all threads that the data has changed
		for (auto *thread = head; thread != nullptr; thread = thread->get_next()) {
			thread->invalidate();
		}
	}

	// Get the master data
	T const &base_data() const {
		return data;
	}

private:
	// The main data that is replicated to threads
	T data;

	// The head of threads that access this replicator. Changes to 'data'
	// will be replicated threads reachable from here
	inline static thread_data *head{};

	// Mutex to serialize access to 'data' when it is modified
	inline static std::shared_mutex mtx;
};
} // namespace tls
#endif // !TLS_REPLICATE_H
