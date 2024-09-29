//
// Created by Croc Dialer on 28.09.24.
//

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>

#include <crocore/utils.hpp>
#include <vierkant/hash.hpp>

namespace vierkant
{

template<typename K, typename V>
class linear_hashmap
{
public:
    using key_t = K;
    using value_t = V;
    using hash_fn = std::function<uint64_t(uint64_t)>;
    static_assert(key_t() == key_t(), "key_t not comparable");

    linear_hashmap() = default;
    linear_hashmap(const linear_hashmap &) = delete;
    linear_hashmap(linear_hashmap &other) : linear_hashmap() { swap(*this, other); };
    linear_hashmap &operator=(linear_hashmap other)
    {
        swap(*this, other);
        return *this;
    }

    explicit linear_hashmap(uint64_t min_capacity)
        : m_capacity(crocore::next_pow_2(min_capacity)), m_storage(std::make_unique<storage_item_t[]>(m_capacity))
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

    value_t &put(const key_t &key, value_t value)
    {
        if(m_num_elements >= m_capacity) { throw std::overflow_error("capacity overflow"); }
        std::shared_lock lock(m_mutex);

        for(uint64_t idx = hash(key);; idx++)
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
            return item.value = value;
        }
    }

    [[nodiscard]] std::optional<value_t> get(const key_t &key) const
    {
        std::shared_lock lock(m_mutex);

        if(!m_capacity) { return {}; }
        for(uint64_t idx = hash(key);; idx++)
        {
            idx &= m_capacity - 1;
            auto &item = m_storage[idx];
            if(!item.key) { return {}; }
            else if(key == item.key) { return item.value; }
        }
    }

    [[nodiscard]] inline bool contains(const key_t &key) const { return get(key) != std::nullopt; }

    const uint8_t *storage() const { return reinterpret_cast<const uint8_t *>(m_storage.get()); }

    size_t storage_num_bytes() const { return sizeof(storage_item_t) * m_capacity; }

    void resize(size_t new_capacity)
    {
        if(new_capacity > m_capacity)
        {
            auto new_linear_hashmap = linear_hashmap(new_capacity);
            storage_item_t *ptr = m_storage.get(), *end = ptr + m_capacity;
            for(; ptr != end; ++ptr) { new_linear_hashmap.put(ptr->key, ptr->value); }
            swap(*this, new_linear_hashmap);
        }
    }

    friend void swap(linear_hashmap &lhs, linear_hashmap &rhs)
    {
        std::lock(lhs.m_mutex, rhs.m_mutex);
        std::unique_lock lock_lhs(lhs.m_mutex, std::adopt_lock), lock_rhs(rhs.m_mutex, std::adopt_lock);
        std::swap(lhs.m_capacity, rhs.m_capacity);
        lhs.m_num_elements = rhs.m_num_elements.exchange(lhs.m_num_elements);
        std::swap(lhs.m_storage, rhs.m_storage);
        std::swap(lhs.m_hash_fn, rhs.m_hash_fn);
    }

private:
    struct alignas(16) storage_item_t
    {
        std::atomic<key_t> key;
        value_t value{};
    };
    static_assert(sizeof(storage_item_t) == sizeof(key_t) + sizeof(value_t),
                  "alignment/size requirements not met for key_t");

    inline uint64_t hash(const key_t &key) const
    {
        constexpr uint32_t num_hashes = sizeof(key_t) / sizeof(uint64_t);
        constexpr uint32_t num_excess_bytes = sizeof(key_t) % sizeof(uint64_t);
        auto ptr = reinterpret_cast<const uint64_t *>(&key), end = ptr + num_hashes;
        size_t h = 0;
        for(; ptr != end; ++ptr) { vierkant::hash_combine(h, m_hash_fn(*ptr)); }
        if constexpr(num_excess_bytes)
        {
            auto end_u8 = reinterpret_cast<const uint8_t *>(end);
            uint64_t tail = 0;
            for(uint32_t i = 0; i < num_excess_bytes; ++i) { tail |= end_u8[i] << (i * 8); }
            vierkant::hash_combine(h, m_hash_fn(tail));
        }
        return h;
    }
    uint64_t m_capacity{};
    std::atomic<uint64_t> m_num_elements;
    std::unique_ptr<storage_item_t[]> m_storage;
    hash_fn m_hash_fn = vierkant::murmur3_fmix64;
    mutable std::shared_mutex m_mutex;
};
}// namespace vierkant
