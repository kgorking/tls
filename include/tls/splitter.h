#ifndef TLS_SPLITTER_H
#define TLS_SPLITTER_H

#include <mutex>
#include <vector>
#include <forward_list>
#include <algorithm>
#ifdef _MSC_VER
#include <new>  // for std::hardware_destructive_interference_size
#endif

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
                }
            }

            T* get_data() {
				return &data;
            }
            T const* get_data() const {
				return &data;
            }

        private:
            T data{};
            splitter<T, UnusedDifferentiaterType>* owner{};
        };
        friend instance_access;

        // Iterator support
        template<class WrappedIterator>
        class wrapper_iterator {
        public:
            // iterator traits
            using difference_type = ptrdiff_t;
            using value_type = T;
            using pointer = const value_type*;
            using reference = const value_type&;
            using iterator_category = std::random_access_iterator_tag;

            constexpr wrapper_iterator() noexcept {};

            constexpr wrapper_iterator(WrappedIterator it_) noexcept
                : it(it_) {
            }

            constexpr wrapper_iterator& operator++() {
				it++;
                return *this;
            }

            constexpr wrapper_iterator operator++(int) {
                iterator const retval = *this;
				it++;
                return retval;
            }

            constexpr wrapper_iterator& operator--() {
				--it;
                return *this;
            }

            constexpr wrapper_iterator operator--(int) {
                wrapper_iterator const retval = *this;
				--it;
                return retval;
            }

            constexpr wrapper_iterator& operator+=(difference_type diff) {
				it += diff;
                return *this;
            }

            constexpr wrapper_iterator& operator-=(difference_type diff) {
                it -= diff;
                return *this;
            }

            constexpr wrapper_iterator operator+(difference_type diff) const {
                return it + diff;
            }

            constexpr difference_type operator-(wrapper_iterator other) const {
                return it - other.it;
            }

            constexpr wrapper_iterator operator-(difference_type diff) const {
                return it - diff;
            }

            constexpr bool operator==(wrapper_iterator other) const {
                return it == other.it;
            }

            constexpr bool operator<(wrapper_iterator other) const {
                return it < other.it;
            }

            constexpr bool operator!=(wrapper_iterator other) const {
                return !(*this == other);
            }

            constexpr value_type& operator*() {
                return *(*it)->get_data();
            }

            constexpr value_type& operator*() const {
                return *(*it)->get_data();
            }

            constexpr value_type* operator->() {
                return (*it)->get_data();
            }

            constexpr value_type* operator->() const {
                return (*it)->get_data();
            }

        private:
            WrappedIterator it{};
        };
        using iterator = wrapper_iterator<typename std::vector<instance_access*>::iterator>;
        using const_iterator = wrapper_iterator<typename std::vector<instance_access*>::const_iterator>;

    private:
        // the threads that access this instance
        std::vector<instance_access*> instances;

        // Mutex for serializing access for creating/removing locals
        std::mutex mtx_storage;

    protected:
        // Adds a instance_access and allocates its data.
        // Returns a pointer to the data
        void init_thread(instance_access* t) {
            std::scoped_lock sl(mtx_storage);

            instances.push_back(t);
        }

        // Remove the instance_access. The data itself is preserved
        void remove_thread(instance_access* t) noexcept {
            std::scoped_lock sl(mtx_storage);

            auto it = std::find(instances.begin(), instances.end(), t);
            if (it != instances.end()) {
                std::swap(*it, instances.back());
                instances.pop_back();
            }
        }

    public:
		constexpr splitter() noexcept = default;
		splitter(splitter const &) = delete;
		constexpr splitter(splitter &&) noexcept = default;
		splitter& operator=(splitter const &) = delete;
		constexpr splitter& operator=(splitter &&) noexcept = default;
        ~splitter() noexcept {
            clear();
        }

        iterator begin() noexcept { return instances.begin(); }
        iterator end() noexcept { return instances.end(); }
        const_iterator begin() const noexcept { return instances.begin(); }
        const_iterator end() const noexcept { return instances.end(); }

        // Get the thread-local instance of T
        T& local() {
            thread_local instance_access var{};
            return var.get(this);
        }

        // Clears all the thread instances and data
        void clear() noexcept {
            for (instance_access* instance : instances)
                instance->remove(this);
            instances.clear();
        }

        // Sort the data using std::less<>
        void sort() {
			sort(std::less<>{});
        }

        // Sort the data using supplied predicate 'bool(auto const&, auto const&)'
        template <class Predicate>
        void sort(Predicate&& pred) {
			std::sort(begin(), end(), std::forward<Predicate>(pred));
        }
    };
}

#endif // !TLS_SPLITTER_H
