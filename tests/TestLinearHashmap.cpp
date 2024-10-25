#include <gtest/gtest.h>
#include <vierkant/linear_hashmap.hpp>

template<template<typename, typename> class hashmap_t>
void test_empty()
{
    hashmap_t<uint64_t, uint32_t> hashmap;
    EXPECT_TRUE(hashmap.empty());
    hashmap.clear();
    EXPECT_EQ(hashmap.capacity(), 0);
    EXPECT_EQ(hashmap.get_storage(nullptr), 0);
};

template<template<typename, typename> class hashmap_t>
void test_basic()
{
    constexpr uint32_t test_capacity = 100;
    hashmap_t<uint64_t, uint64_t> hashmap(test_capacity);
    EXPECT_TRUE(hashmap.empty());
    EXPECT_GT(hashmap.get_storage(nullptr), 0);

    // capacity will be rounded to next pow2
    EXPECT_GE(hashmap.capacity(), test_capacity);
    EXPECT_TRUE(crocore::is_pow_2(hashmap.capacity()));

    EXPECT_FALSE(hashmap.contains(0));
    EXPECT_FALSE(hashmap.contains(13));
    EXPECT_FALSE(hashmap.contains(42));

    hashmap.put(69, 99);
    hashmap.put(13, 12);
    hashmap.put(8, 15);
    EXPECT_EQ(hashmap.size(), 3);

    hashmap.remove(8);
    EXPECT_EQ(hashmap.size(), 2);
    EXPECT_FALSE(hashmap.contains(8));

    EXPECT_TRUE(hashmap.contains(69));
    EXPECT_EQ(hashmap.get(69), 99);
    EXPECT_TRUE(hashmap.contains(13));
    EXPECT_EQ(hashmap.get(13), 12);

    auto storage = std::make_unique<uint8_t[]>(hashmap.get_storage(nullptr));
    hashmap.get_storage(storage.get());
}

template<template<typename, typename> class hashmap_t>
void test_custom_key()
{
    // custom 32-byte key
    struct custom_key_t
    {
        int v[8]{};
        constexpr bool operator==(const custom_key_t &other) const
        {
            for(uint32_t i = 0; i < 8; ++i)
            {
                if(v[i] != other.v[i]) { return false; }
            }
            return true;
        }
    };
    constexpr uint32_t test_capacity = 100;
    auto hashmap = hashmap_t<custom_key_t, uint64_t>(test_capacity);

    custom_key_t k1{{1, 2, 3, 4, 5, 6, 7, 8}};
    hashmap.put(k1, 69);
    EXPECT_TRUE(hashmap.contains(k1));
    EXPECT_FALSE(hashmap.contains(custom_key_t()));
}

template<template<typename, typename> class hashmap_t>
void test_probe_length()
{
    hashmap_t<uint32_t, uint32_t> hashmap;

    // default load_factor is 0.5
    EXPECT_EQ(hashmap.max_load_factor(), 0.5f);

    // test a load-factor of 0.25
    hashmap.max_load_factor(0.25f);

    constexpr uint32_t test_capacity = 512;
    constexpr uint32_t num_insertions = 128;
    hashmap.reserve(test_capacity);

    float probe_length_sum = 0.f;
    for(uint32_t i = 0; i < num_insertions; i++) { probe_length_sum += static_cast<float>(hashmap.put(i, 69)); }
    float avg_probe_length = probe_length_sum / num_insertions;

    // for a load-factor of 0.25, we expect very short probe-lengths
    constexpr float expected_max_avg_probe_length = 0.15f;
    EXPECT_LE(avg_probe_length, expected_max_avg_probe_length);

    EXPECT_LE(hashmap.load_factor(), 0.25f);
}

TEST(linear_hashmap, empty)
{
    test_empty<vierkant::linear_hashmap>();
    test_empty<vierkant::linear_hashmap_mt>();
}

TEST(linear_hashmap, basic)
{
    test_basic<vierkant::linear_hashmap>();
    test_basic<vierkant::linear_hashmap_mt>();
}

TEST(linear_hashmap, custom_key)
{
    test_custom_key<vierkant::linear_hashmap>();
    test_custom_key<vierkant::linear_hashmap_mt>();
}

template<template<typename, typename> class hashmap_t>
void test_reserve()
{
    hashmap_t<uint64_t, uint64_t> hashmap;

    // fix by resizing
    hashmap.reserve(17);
    EXPECT_TRUE(hashmap.empty());
    hashmap.put(13, 12);
    EXPECT_TRUE(hashmap.contains(13));

    // empty / no capacity specified -> triggers internal resize
    hashmap = {};
    hashmap.put(13, 12);
    EXPECT_TRUE(hashmap.contains(13));
}

TEST(linear_hashmap, reserve)
{
    test_reserve<vierkant::linear_hashmap>();
    test_reserve<vierkant::linear_hashmap_mt>();
}

TEST(linear_hashmap, probe_length)
{
    test_probe_length<vierkant::linear_hashmap>();
    test_probe_length<vierkant::linear_hashmap_mt>();
}