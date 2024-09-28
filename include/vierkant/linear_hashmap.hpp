//
// Created by Croc Dialer on 28.09.24.
//

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

#include <crocore/utils.hpp>
#include <vierkant/hash.hpp>

namespace vierkant
{

class linear_hashmap
{
public:
    using key_t = uint64_t;
    using value_t = uint64_t;
    using hash_fn = std::function<uint64_t(key_t)>;

    linear_hashmap() = default;

    explicit linear_hashmap(uint64_t capacity)
        : m_capacity(crocore::next_pow_2(capacity)), m_storage(std::make_unique<storage_item_t[]>(m_capacity))
    {
        clear();
    }

    inline size_t size() const { return m_num_elements; }

    inline size_t capacity() const { return m_capacity; }

    inline bool empty() const { return size() == 0; }

    inline void clear()
    {
        storage_item_t *ptr = m_storage.get(), *end = ptr + m_capacity;
        for(; ptr != end; ++ptr) { ptr->key = 0; }
    }

    void insert(const key_t &key, value_t value)
    {
        assert(m_capacity);
        for(uint64_t idx = m_hash_fn(key);; idx++)
        {
            idx &= m_capacity - 1;
            auto &item = m_storage[idx];

            // load previous key
            key_t probed_key = m_storage[idx].key;

            if(probed_key != key)
            {
                // hit another entry, keep searching
                if(probed_key != 0) { continue; }

                item.key.compare_exchange_strong(probed_key, key, std::memory_order_relaxed, std::memory_order_relaxed);
                if(probed_key && probed_key != key)
                {
                    // another thread just stole it
                    continue;
                }
                m_num_elements++;
            }
            item.value = value;
            return;
        }
    }

    [[nodiscard]] std::optional<value_t> get(const key_t &key) const
    {
        assert(m_capacity);
        for(uint64_t idx = m_hash_fn(key);; idx++)
        {
            idx &= m_capacity - 1;
            auto &item = m_storage[idx];
            if(!item.key) { return {}; }
            else if(key == item.key) { return item.value; }
        }
    }

    [[nodiscard]] inline bool contains(const key_t &key) const { return get(key) != std::nullopt; }

private:
    struct storage_item_t
    {
        std::atomic<key_t> key;
        value_t value{};
    };
    uint64_t m_capacity{};
    std::atomic<uint64_t> m_num_elements;
    std::unique_ptr<storage_item_t[]> m_storage;
    hash_fn m_hash_fn = vierkant::murmur3_fmix64;
};
}// namespace vierkant
