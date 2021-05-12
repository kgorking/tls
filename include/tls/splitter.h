#ifndef TLS_SPLITTER_H
#define TLS_SPLITTER_H

#include <mutex>

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
            ~instance_access() {
                if (owner != nullptr) {
					owner->remove_thread(this);
                }
            }

            // Return a reference to an instances local data
            T& get(splitter<T, UnusedDifferentiaterType>* instance) {
                // If the owner is null, (re-)initialize the instance.
                // Data may still be present if the thread_local instance is still active
				if (owner == nullptr) {
					data = {};
					owner = instance;
					instance->init_thread(this);
				}
                return data;
            }

            void remove(splitter<T, UnusedDifferentiaterType>* instance) noexcept {
                if (owner == instance) {
					data = {};
                    owner = nullptr;
					next = nullptr;
                }
            }

            T* get_data() {
				return &data;
            }
            T const* get_data() const {
				return &data;
            }

            void set_next(instance_access *ia) {
				next = ia;
            }

            instance_access* get_next() {
				return next;
            }

            instance_access const* get_next() const {
				return next;
            }

        private:
            T data{};
            splitter<T, UnusedDifferentiaterType>* owner{};
			instance_access *next = nullptr;
        };
        friend instance_access;

        // Iterator support
        class iterator {
        public:
            // iterator traits
            using difference_type = ptrdiff_t;
            using value_type = T;
            using pointer = const value_type*;
            using reference = const value_type&;
            using iterator_category = std::forward_iterator_tag;

            constexpr iterator() noexcept = default;

            constexpr iterator(instance_access *inst_) noexcept
                : inst(inst_) {
            }

            constexpr iterator(instance_access const* inst_) noexcept
                : inst(inst_) {
            }

            constexpr iterator& operator++() {
				inst = inst->get_next();
                return *this;
            }

            constexpr iterator operator++(int) {
                iterator const retval = *this;
				inst = inst->get_next();
                return retval;
            }

            constexpr bool operator==(iterator other) const {
                return inst == other.inst;
            }

            constexpr bool operator!=(iterator other) const {
                return !(*this == other);
            }

            constexpr value_type& operator*() {
                return *inst->get_data();
            }

            constexpr value_type* operator->() {
                return inst->get_data();
            }

            constexpr value_type& operator*() const {
                return *inst->get_data();
            }

            constexpr value_type* operator->() const {
                return inst->get_data();
            }
        private:
            instance_access *inst{};
        };
        class const_iterator {
        public:
            // iterator traits
            using difference_type = ptrdiff_t;
            using value_type = const T;
            using pointer = const value_type*;
            using reference = const value_type&;
            using iterator_category = std::forward_iterator_tag;

            constexpr const_iterator() noexcept = default;

            constexpr const_iterator(instance_access const* inst_) noexcept
                : inst(inst_) {
            }

            constexpr const_iterator& operator++() {
				inst = inst->get_next();
                return *this;
            }

            constexpr const_iterator operator++(int) {
                const_iterator const retval = *this;
				inst = inst->get_next();
                return retval;
            }

            constexpr bool operator==(const_iterator other) const {
                return inst == other.inst;
            }

            constexpr bool operator!=(const_iterator other) const {
                return !(*this == other);
            }

            constexpr value_type& operator*() {
                return *inst->get_data();
            }

            constexpr value_type* operator->() {
                return inst->get_data();
            }

            constexpr value_type& operator*() const {
                return *inst->get_data();
            }

            constexpr value_type* operator->() const {
                return inst->get_data();
            }

        private:
            instance_access const* inst{};
        };

    private:
        // the head of the threads that access this splitter instance
		instance_access *head{};

        // Mutex for serializing access for adding/removing thread-local instances
        std::mutex mtx_storage;

    protected:
        // Adds a instance_access and allocates its data.
        // Returns a pointer to the data
        void init_thread(instance_access* t) {
            std::scoped_lock sl(mtx_storage);

            t->set_next(head);
			head = t;
        }

        // Remove the instance_access. The data itself is preserved
        void remove_thread(instance_access* t) noexcept {
			if (head == nullptr)
				return;

            std::scoped_lock sl(mtx_storage);

            if (head == t) {
				head = t->get_next();
			} else {
				auto curr = head;
				while (curr != nullptr) {
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
		splitter& operator=(splitter const &) = delete;
		splitter& operator=(splitter &&) noexcept = default;
        ~splitter() noexcept {
            clear();
        }

        iterator begin() noexcept { return iterator{head}; }
        iterator end() noexcept { return iterator{}; }
        const_iterator begin() const noexcept { return const_iterator{head}; }
        const_iterator end() const noexcept { return const_iterator{}; }

        // Get the thread-local instance of T
        T& local() {
            thread_local instance_access var{};
            return var.get(this);
        }

        // Clears all the thread instances and data
        void clear() noexcept {
			for (instance_access *instance = head; instance != nullptr; ) {
				auto next = instance->get_next();
				instance->remove(this);
				instance = next;
			}

			head = nullptr;
        }
    };
}

#endif // !TLS_SPLITTER_H
