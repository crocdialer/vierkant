module hashmap;

import "hash.slang";

namespace utils
{

public interface INullable
{
    static This zero();
}

public interface IComparable
{
    bool equals(This other);
}

public interface IHashable
{
    uint hash();
}

public interface IHashKey : INullable,
                            IComparable,
                            IHashable
{};

interface IHashValue : INullable,
                       IComparable
{};

// Implement for uint
extension uint : INullable
{
    static uint zero() { return 0u; }
}

extension uint : IComparable
{
    bool equals(uint other) { return this == other; }
}

extension uint : IHashable
{
    uint hash() { return utils::xxhash32(this, 0); }
}

extension uint : IHashKey {};
extension uint : IHashValue {};

public struct hashmap_t<K : IHashKey, V : IHashValue>
{
    struct item_t
    {
        K key;
        V value;
    };

    item_t *storage;
    uint size;
    uint capacity;

    public V get(K key)
    {
        if(key.equals(K.zero()) || capacity == 0) { return V.zero(); }

        for(uint idx = key.hash();; idx++)
        {
            idx &= capacity - 1;
            item_t *item = storage + idx;

            if(item->key.equals(K.zero())) { return V.zero(); }
            else if(key.equals(item->key)) { return item->value; }
        }
    }

    public bool contains(K key) { return !get(key).equals(V.zero()); }
};
}
