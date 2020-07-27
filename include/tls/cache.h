#ifndef __TLS_CACHE
#define __TLS_CACHE

#include <algorithm>

namespace tls {
    // A class using a cache-line to cache data
    template <class Key, class Value, Key empty_slot = Key{}, size_t cache_line = 64UL>
    class cache {
    public:
        static constexpr size_t max_entries = (cache_line) / (sizeof(Key) + sizeof(Value));

        cache() noexcept {
            reset();
        }

        template <class Fn>
        Value get_or(Key const& k, Fn or_fn) {
            auto const index = find_index(k);
            if (index < max_entries)
                return values[index];

            insert_val(k, or_fn(k));
            return values[0];
        }

        void reset() {
            std::fill(keys, keys + max_entries, empty_slot);
            std::fill(values, values + max_entries, Value{});
        }

    protected:
        void insert_val(Key const& k, Value v) {
            // Move all but last pair one step to the right
            //std::shift_right(keys, keys + max_entries, 1);
            //std::shift_right(values, values + max_entries, 1);
            std::move_backward(keys, keys + max_entries - 1, keys + max_entries);
            std::move_backward(values, values + max_entries - 1, values + max_entries);

            // Insert the new pair at the front of the cache
            keys[0] = k;
            values[0] = std::move(v);
        }

        std::size_t find_index(Key const& k) const {
            auto const it = std::find(keys, keys + max_entries, k);
            if (it == keys + max_entries)
                return max_entries;
            return std::distance(keys, it);
        }

    private:
        Key keys[max_entries];
        Value values[max_entries];
    };

}

#endif // !__TLS_CACHE
