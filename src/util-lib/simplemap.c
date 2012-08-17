/*
 * simplemap.c
 *
 *  Created on: 31 Jul 2012
 *      Author: kant
 */
#include <assert.h>

#include <util-lib/simplemap.h>
#include <util-lib/fast_hash.h>

/**
\brief Creates a simple map.
*/
map_t simplemap_create(uint32_t size)
{
    Print(infoLong, "Creating map of size %d.", size);
    map_t map;
    map.values = RTmalloc(size*sizeof(entry_t));
    for(size_t i = 0; i < size; i++)
    {
        map.values[i].key = 0;
        map.values[i].value = 0;
    }
    map.size = size;
    return map;
}

void simplemap_destroy(map_t map)
{
    RTfree(map.values);
}

static inline uint32_t hash(uint32_t value)
{
    //return value;
    return SuperFastHash(&value, sizeof(uint32_t), 0);
}

/**
\brief Puts an element for the key.
\returns true if the element is inserted, false if it was already present.
\throws Abort if the map if full.
*/
bool simplemap_put(map_t map, uint32_t key, uint32_t value)
{
    uint32_t loc = hash(key) % map.size;
    assert(loc < map.size);
    //Print(infoLong, "simplemap_put: key=%d, value=%d, loc=%d.", key, value, loc);
    uint32_t l = loc;
    uint32_t c = 0;
    while (map.values[loc].value&0x80000000 && map.values[loc].key != key)
    {
        if (loc < map.size - 1)
        {
            loc++;
        }
        else
        {
            loc = 0;
        }
        if (loc==l)
        {
            Print(infoLong, "simplemap_put: loc=%d, c=%d.", loc, c);
            Abort("Map is full.");
        }
        c++;
        assert(loc < map.size);
    }
    if (map.values[loc].value & 0x80000000) // => map->values[loc].key == key
    {
        assert((map.values[loc].value & 0x7fffffff) == value);
        return false;
    }
    else {
        map.values[loc].value = value | 0x80000000;
        map.values[loc].key = key;
        return true;
    }
}

/**
\brief Gets the element for the key.
*/
uint32_t simplemap_get(map_t map, uint32_t key)
{
    uint32_t loc = hash(key) % map.size;
    uint32_t l = loc;
    while (map.values[loc].value & 0x80000000 && map.values[loc].key != key)
    {
        if (loc < map.size - 1)
        {
            loc++;
        }
        else
        {
            loc = 0;
        }
        if (loc==l)
        {
            Abort("Key not found.");
        }
    }
    if (map.values[loc].value & 0x80000000) // => map->values[loc].key == key
    {
        return map.values[loc].value & 0x7fffffff;
    }
    else {
        Abort("Key not found.");
    }
}

void simplemap_test()
{
    Print(infoLong, "Start of simplemap testsuite.");
    map_t map1 = simplemap_create(10);
    simplemap_put(map1, 5, 3);
    assert(simplemap_get(map1, 5) == 3);
    simplemap_put(map1, 2, 4);
    assert(simplemap_get(map1, 2) == 4);
    simplemap_put(map1, 1, 3);
    assert(simplemap_get(map1, 1) == 3);
    simplemap_put(map1, 200, 11);
    assert(simplemap_get(map1, 200) == 11);
    assert(simplemap_get(map1, 5) == 3);

    map_t map2 = simplemap_create(100);
    for(uint32_t i = 0; i < 100; i++)
    {
        simplemap_put(map2, i, 100-i);
        assert(simplemap_get(map2, i) == 100-i);
    }
    simplemap_destroy(map1);
    simplemap_destroy(map2);
    Print(infoLong, "End of simplemap testsuite.");
}


/**
\brief Creates a simple map.
*/
map64_t simplemap64_create(uint64_t size)
{
    Print(infoLong, "Creating map of size %d.", size);
    map64_t map;
    map.values = RTmalloc(size*sizeof(entry64_t));
    for(size_t i = 0; i < size; i++)
    {
        map.values[i].key = 0;
        map.values[i].value = 0;
    }
    map.size = size;
    return map;
}

void simplemap64_destroy(map64_t map)
{
    RTfree(map.values);
}

static inline uint64_t hash64(uint64_t value)
{
    //return value;
    return MurmurHash64(&value, sizeof(uint64_t), 0);
}

/**
\brief Puts an element for the key.
\returns true if the element is inserted, false if it was already present.
\throws Abort if the map if full.
*/
bool simplemap64_put(map64_t map, uint64_t key, uint64_t value)
{
    uint64_t loc = hash64(key) % map.size;
    assert(loc < map.size);
    //Print(infoLong, "simplemap_put: key=%d, value=%d, loc=%d.", key, value, loc);
    uint64_t l = loc;
    uint64_t c = 0;
    while (map.values[loc].value&0x80000000 && map.values[loc].key != key)
    {
        if (loc < map.size - 1)
        {
            loc++;
        }
        else
        {
            loc = 0;
        }
        if (loc==l)
        {
            Print(infoLong, "simplemap_put: loc=%d, c=%d.", loc, c);
            Abort("Map is full.");
        }
        c++;
        assert(loc < map.size);
    }
    if (map.values[loc].value & 0x80000000) // => map->values[loc].key == key
    {
        assert((map.values[loc].value & 0x7fffffff) == value);
        return false;
    }
    else {
        map.values[loc].value = value | 0x80000000;
        map.values[loc].key = key;
        return true;
    }
}

/**
\brief Gets the element for the key.
*/
uint64_t simplemap64_get(map64_t map, uint64_t key)
{
    uint64_t loc = hash64(key) % map.size;
    uint64_t l = loc;
    while (map.values[loc].value & 0x80000000 && map.values[loc].key != key)
    {
        if (loc < map.size - 1)
        {
            loc++;
        }
        else
        {
            loc = 0;
        }
        if (loc==l)
        {
            Abort("Key not found.");
        }
    }
    if (map.values[loc].value & 0x80000000) // => map->values[loc].key == key
    {
        return map.values[loc].value & 0x7fffffff;
    }
    else {
        Abort("Key not found.");
    }
}