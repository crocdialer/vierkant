module hashmap;

import integer_traits;

namespace utils
{

public struct hashmap_t<K, V>
        where K : INullable, IComparable, IHashable, IAtomicable
        where V : INullable, IComparable
{
    struct item_t
    {
        K key;
        V value;
    };

    item_t *storage;
    uint size;
    uint capacity;

    public V get(const K key)
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

    [mutating]
    public void put(const K key, const V value)
    {
        if(key.equals(K.zero()) || capacity == 0) { return; }

        for(uint idx = key.hash();; idx++)
        {
            idx &= capacity - 1;
            item_t *item = storage + idx;
            K probed_key = item->key;

            if(!probed_key.equals(key))
            {
                // hit another valid entry, keep probing
                if(!probed_key.equals(K.zero()) && !item->value.equals(V.zero())) { continue; }

                // try to insert atomically
                InterlockedCompareExchange<K>(item->key, probed_key, key, probed_key);
                if(!probed_key.equals(K.zero()) && !probed_key.equals(key))
                {
                    // another thread just stole it
                    continue;
                }
                // atomically increase size
                InterlockedAdd<uint>(size, 1u);
            }
            item->value = value;
            return;
        }
    }

    public bool contains(K key) { return !get(key).equals(V.zero()); }
};
}
