#include "chain_hash.h"
#include "MurMurHash3.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define PREFIX_COOKIE "\x23\x23\x32\x32\x42\x42\x87\x89"
#define POSTFIX_COOKIE "\x32\x32\x23\x23\x24\x24\x78\x98"

#ifdef __CHAIN_HASHTABLE_FOR_IPC__
#pragma pack(1)
#endif

struct chain_hash_entry_t {
    uint32_t index;

    int32_t next;
#ifdef __CHAIN_HASHTABLE_LRU__
    int32_t prev;
    int32_t lru_next;
    int32_t lru_prev;
#endif

#ifdef __CHAIN_HASHTABLE_CNT_LRU__
    // make sure following is 4 bytes aligned.
    uint32_t lru_timestamp;
#endif

#ifdef __CHAIN_HASHTABLE_FOR_IPC__
    uint32_t ref;
    uint32_t seed;
#endif

    char key[0];
};

struct chain_hash_bucket_t {
    int32_t entry_head;
#ifdef _UNIT_TEST_
    uint32_t num_entry;
#endif
};

#ifdef __CHAIN_HASHTABLE_FOR_IPC__
#pragma pack(0)
#endif

static int32_t chain_hash_lookup_key_with_hash(struct chain_hash_t* ht, const void* key, uint32_t hash, uint32_t tm);

void* __attribute__((weak)) chain_hash_mem_alloc(uint32_t size) {
    return 0;
}

void __attribute__((weak)) chain_hash_mem_free(void* p) {
    assert(0);
}

void __attribute__((weak)) chain_hash_log_error(int type, const char* msg) {
    // TODO
}

void __attribute__((weak)) chain_hash_log_dbg(int type, const char* msg) {
    // TODO
}

void __attribute__((weak)) chain_hash_log_warn(int type, const char* msg) {
    // TODO
}

static int32_t chain_hash_cmp_4_byte(const void* k1, const void* k2, size_t len) {
    assert(len == 4);
    return *(int32_t*)k1 - *(int32_t*)k2;
}

static uint32_t chain_hash_func_4_byte(const void* key, uint32_t len) {
    assert(len == 4);
    return *(uint32_t*)key;
}

static void* chain_hash_copy_4_byte(void* dst, const void* src, size_t len) {
    *(uint32_t*)dst = *(const uint32_t*)src;
    return dst;
}

inline __attribute__((always_inline)) uint32_t chain_hash_func_murmur_hash(uint32_t seed, const void* key, uint32_t len) {
    uint32_t hash = 0;
    MurmurHash3_x86_32(key, len, seed, &hash);
    return hash;
}

inline __attribute__((always_inline)) uint32_t chain_hash_func_murmur_hash2(const void* key, uint32_t len) {
    uint32_t hash = 0;
    MurmurHash3_x86_32(key, len, 2333, &hash);
    return hash;
}

// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static inline uint32_t round_to_power_of_2(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

#define get_hash_entry(list, idx, key_len) (struct chain_hash_entry_t*)((char*)(list) + (idx) * (sizeof(struct chain_hash_entry_t) + (key_len)))
#define get_hash_entry_list(ht) (struct chain_hash_entry_t*)((char*)(ht) + (ht)->entry_list_off_)
#define get_hash_bucket_list(ht) (struct chain_hash_bucket_t*)((char*)(ht) + (ht)->bucket_list_off_)

static void extract_from_list(struct chain_hash_t* ht, int bucket, int idx, int prev) {
    struct chain_hash_entry_t* entrys = get_hash_entry_list(ht);
    struct chain_hash_bucket_t* buckets = get_hash_bucket_list(ht);

    struct chain_hash_entry_t* entry = get_hash_entry(entrys, idx, ht->key_len);
    int next = entry->next;

#ifdef __CHAIN_HASHTABLE_LRU__
    prev = entry->prev;
    if (next >= 0) {
        struct chain_hash_entry_t* next_ent = get_hash_entry(entrys, next, ht->key_len);
        next_ent->prev = entry->prev;
    }
#endif

    if (prev >= 0) {
        struct chain_hash_entry_t* prev_ent = get_hash_entry(entrys, prev, ht->key_len);
        prev_ent->next = next;
    } else if (buckets[bucket].entry_head == idx) {
        buckets[bucket].entry_head = next;
    } else {
        // error.
        assert(0);
    }

    entry->next = -1;
#ifdef __CHAIN_HASHTABLE_LRU__
    entry->prev = -1;
#endif
}

static void insert_to_list(struct chain_hash_t* ht, int bucket, int idx) {
    struct chain_hash_entry_t* entrys = get_hash_entry_list(ht);
    struct chain_hash_bucket_t* buckets = get_hash_bucket_list(ht);
    struct chain_hash_entry_t* entry = get_hash_entry(entrys, idx, ht->key_len);

    const int head = buckets[bucket].entry_head;
    entry->next = head;
    buckets[bucket].entry_head = idx; // public change first.

#ifdef __CHAIN_HASH_SYNC__
    __sync_synchronize();
#endif

    ht->occupied++;

#ifdef __CHAIN_HASHTABLE_LRU__
    entry->prev = -1;
    if (head < 0) return;

    struct chain_hash_entry_t *head_ent = get_hash_entry(entrys, head, ht->key_len);
    head_ent->prev = idx;
#endif
}

uint32_t chain_hash_calc_mem(uint32_t num_bucket, uint32_t num_entry, uint32_t key_len) {
#ifdef __CHAIN_HASHTABLE_CNT_LRU__
    // key must be 4 bytes aligned.
    assert(key_len%4==0);
#endif
    num_bucket = round_to_power_of_2(num_bucket);
    const uint32_t total_sz = sizeof(struct chain_hash_t) + sizeof(struct chain_hash_bucket_t) * num_bucket + num_entry * (sizeof(struct chain_hash_entry_t) + key_len);

    // aligned to 8 bytes
    return ((total_sz + 7)/8) * 8;
}

struct chain_hash_t* chain_hash_create_with_mem(uint32_t num_bucket, uint32_t num_entries, uint32_t key_len, void* mem, uint32_t len) {
    num_bucket = round_to_power_of_2(num_bucket);
    uint32_t total_sz = chain_hash_calc_mem(num_bucket, num_entries, key_len);
    if (total_sz > len) return NULL;

    struct chain_hash_t* ht = (struct chain_hash_t*)mem;

    ht->num_bucket = num_bucket;
    ht->num_entries = num_entries;

    ht->occupied = 0;
    ht->key_len = key_len;
    ht->hash_mask = num_bucket - 1;

    ht->free_list = 0;
    ht->bucket_list_off_ = sizeof(struct chain_hash_t);
    ht->entry_list_off_ = sizeof(struct chain_hash_t) + num_bucket * sizeof(struct chain_hash_bucket_t);
    // ht->entry_list = (struct chain_hash_entry_t*)((char*)ht + sizeof(struct chain_hash_t) + num_bucket * sizeof(struct chain_hash_bucket_t));
    // ht->hash_bucket = (struct chain_hash_bucket_t*)((char*)ht + sizeof(struct chain_hash_t));

#ifdef __CHAIN_HASHTABLE_LRU__
    ht->lru_list_head = -1;
#endif

#ifdef __CHAIN_HASHTABLE_CNT_LRU__
    ht->lru_timestamp = 0;
#endif

#ifndef __CHAIN_HASHTABLE_FOR_IPC__
    switch(key_len) {
    case 4:
        ht->cmper = chain_hash_cmp_4_byte;
        ht->copyer = chain_hash_copy_4_byte;
        ht->hasher = chain_hash_func_4_byte;
        break;
    default:
        ht->cmper = memcmp;
        ht->hasher = chain_hash_func_murmur_hash2;
        ht->copyer = memcpy;
        break;
    };
#else
    assert(sizeof(PREFIX_COOKIE) - 1 == sizeof(ht->cookie_prefix));
    assert(sizeof(POSTFIX_COOKIE) - 1 == sizeof(ht->cookie_postfix));
    memcpy(ht->cookie_prefix, PREFIX_COOKIE, sizeof(ht->cookie_prefix));
    memcpy(ht->cookie_postfix, POSTFIX_COOKIE, sizeof(ht->cookie_postfix));
#endif

    struct chain_hash_bucket_t* buckets = get_hash_bucket_list(ht);

    int i = 0;
    for (i = 0; i < num_bucket; ++i) {
        buckets[i].entry_head = -1;
#ifdef _UNIT_TEST_
        buckets[i].num_entry = 0;
#endif
    }

    struct chain_hash_entry_t* entry = 0;
    struct chain_hash_entry_t* entrys = get_hash_entry_list(ht);
    for (i = 0; i < num_entries; ++i) {
        entry = get_hash_entry(entrys, i, key_len);
        entry->index = i;
        entry->next = i + 1;
#ifdef __CHAIN_HASHTABLE_LRU__
        entry->prev = i;
        entry->lru_next = entry->lru_prev = -1;
#endif

#ifdef __CHAIN_HASHTABLE_FOR_IPC__
        entry->ref = 0;
        entry->seed = 0;
#endif
    }

    entry = get_hash_entry(entrys, num_entries - 1, key_len);
    entry->next = -1; // num_entries >= 2

    return ht;
}

#ifdef __CHAIN_HASHTABLE_FOR_IPC__
int32_t chain_hash_recycle_stray_entry(struct chain_hash_t *ht) {
    struct chain_hash_entry_t* entry = 0;
    struct chain_hash_entry_t* entrys = get_hash_entry_list(ht);
    struct chain_hash_bucket_t* buckets = get_hash_bucket_list(ht);

    int32_t i, idx, total = 0;
    for (i = 0; i < ht->num_bucket && total < ht->num_entries + 1; ++i) {
        idx = buckets[i].entry_head;

        while (idx >= 0 && total < ht->num_entries + 1) {
            ++total;
            entry = get_hash_entry(entrys, idx, ht->key_len);
            entry->ref++;
            idx = entry->next;
        }
    }

    idx = ht->free_list;
    while (idx >= 0 && total < ht->num_entries + 1) {
        ++total;
        entry = get_hash_entry(entrys, idx, ht->key_len);
        entry->ref++;
        idx = entry->next;
    }

    if (total == ht->num_entries + 1) {
        chain_hash_log_error(CHET_LOOP_IN_ENTRY_LIST, "recycle stray entry failed, loop in list");
        return -1;
    }

    int32_t recycled = 0;
    for (idx = 0; idx < ht->num_entries; ++idx) {
        entry = get_hash_entry(entrys, idx, ht->key_len);
        if (entry->ref) {
            entry->ref = 0;
        } else {
            entry->next = ht->free_list;
            ht->free_list = i;
            recycled++;
        }
    }

    return recycled;
}
#endif

struct chain_hash_t* chain_hash_create(uint32_t num_bucket, uint32_t num_entries, uint32_t key_len) {
    // assert(num_bucket & (num_bucket - 1) == 0);

    num_bucket = round_to_power_of_2(num_bucket);
    uint32_t total_sz = chain_hash_calc_mem(num_bucket, num_entries, key_len);

    struct chain_hash_t* ht = (struct chain_hash_t*)chain_hash_mem_alloc(total_sz);
    if (!ht) return ht;

    return chain_hash_create_with_mem(num_bucket, num_entries, key_len, ht, total_sz);
}

#ifdef __CHAIN_HASHTABLE_FOR_IPC__
struct chain_hash_t* chain_hash_attach_mem(void* mem, uint32_t len) {
    if (!mem || len < sizeof(struct chain_hash_t)) return 0;

    struct chain_hash_t* ht = (struct chain_hash_t*)mem;

    if (memcmp(ht->cookie_prefix, PREFIX_COOKIE, sizeof(ht->cookie_prefix)) ||
        memcmp(ht->cookie_postfix, POSTFIX_COOKIE, sizeof(ht->cookie_postfix))) {
        chain_hash_log_error(CHET_BAD_COOKIE, "cookie check failed for attaching to existing table.");
        return 0;
    }

    if (ht->num_bucket != round_to_power_of_2(ht->num_bucket)) {
        chain_hash_log_error(CHET_BAD_TABLE_SIZE, "bucket number not power of 2 for attaching to mem");
        return 0;
    }

    uint32_t total_sz = chain_hash_calc_mem(ht->num_bucket, ht->num_entries, ht->key_len);
    if (total_sz > len) {
        chain_hash_log_error(CHET_BAD_TABLE_SIZE, "table size > mem size for attaching to mem");
        return 0;
    }

    return ht;
}
#endif

void chain_hash_destroy(struct chain_hash_t* ht) {
    chain_hash_mem_free(ht);
}

#ifdef __CHAIN_HASHTABLE_LRU__
static void extract_from_lru_list(struct chain_hash_t* ht, struct chain_hash_entry_t* entrys, int idx) {
    struct chain_hash_entry_t* entry = get_hash_entry(entrys, idx, ht->key_len);
    int prev = entry->lru_prev, next = entry->lru_next;

    struct chain_hash_entry_t *next_ent = get_hash_entry(entrys, next, ht->key_len);
    next_ent->lru_prev = entry->lru_prev;

    struct chain_hash_entry_t *prev_ent = get_hash_entry(entrys, prev, ht->key_len);
    prev_ent->lru_next = entry->lru_next;

    if (ht->lru_list_head == idx) {
        if (next == idx) {
            ht->lru_list_head = -1;
        } else {
            ht->lru_list_head = next;
        }
    }

    entry->lru_next = entry->lru_prev = -1;
}

static void insert_to_lru_list(struct chain_hash_t* ht, struct chain_hash_entry_t* entrys, int idx) {
    struct chain_hash_entry_t* entry = get_hash_entry(entrys, idx, ht->key_len);

    const int lru_head = ht->lru_list_head;
    entry->lru_next = lru_head;

    if (lru_head >= 0) {
        struct chain_hash_entry_t* head_ent = get_hash_entry(entrys, lru_head, ht->key_len);
        int lru_tail = head_ent->lru_prev;
        head_ent->lru_prev = idx;

        struct chain_hash_entry_t* tail_ent = get_hash_entry(entrys, lru_tail, ht->key_len);
        tail_ent->lru_next = idx;
        entry->lru_prev = lru_tail;
    } else {
        entry->lru_prev = entry->lru_next = idx;
    }

    ht->lru_list_head = idx;
}
#endif


// we have multiple readers, thus increment better be atomic.
// but since lru_timestamp is 4 bytes aligned, it is ok to use plain add operation.
// lru_timestamp doesn't really need to be exact accurate.
#define INCREMENT_LRU_TM(v) __sync_add_and_fetch(&(v), 1)

int32_t chain_hash_add_key(struct chain_hash_t* ht, const void* key, uint32_t seed) {
#ifndef __CHAIN_HASHTABLE_FOR_IPC__
    const uint32_t hash = ht->hasher(key, ht->key_len);
#else
    const uint32_t hash = chain_hash_func_murmur_hash(seed, key, ht->key_len);
#endif

#ifdef __CHAIN_HASHTABLE_CNT_LRU__
    uint32_t tm = INCREMENT_LRU_TM(ht->lru_timestamp);
#else
    uint32_t tm = 0;
#endif

    int32_t idx = chain_hash_lookup_key_with_hash(ht, key, hash, tm);
    if (idx >= 0) return idx;

    if (ht->free_list < 0) {
        chain_hash_log_error(CHET_NO_FREE_ENTRY, "add key failed, no free entry available");
        return -1;
    }

    struct chain_hash_entry_t* entrys = get_hash_entry_list(ht);

    idx = ht->free_list;

    struct chain_hash_entry_t* new_entry = get_hash_entry(entrys, idx, ht->key_len);

    ht->free_list = new_entry->next; // node lost if writer process get killed after this line

    const uint32_t hash_idx = (hash & ht->hash_mask);

#ifdef _UNIT_TEST_
    struct chain_hash_bucket_t* buckets = get_hash_bucket_list(ht);
    buckets[hash_idx].num_entry++;
#endif

#ifndef __CHAIN_HASHTABLE_FOR_IPC__
    ht->copyer(new_entry->key, key, ht->key_len);
#else
    new_entry->seed = seed;
    memcpy(new_entry->key, key, ht->key_len);
#endif

    // insert into head.
    insert_to_list(ht, hash_idx, idx);

#ifdef __CHAIN_HASHTABLE_LRU__
    insert_to_lru_list(ht, entrys, idx);
#endif

#ifdef __CHAIN_HASHTABLE_CNT_LRU__
    // yep we have race here, but it doesn't really matter, benign race.
    new_entry->lru_timestamp = tm;
#endif

    return new_entry->index;
}

int32_t chain_hash_iterate_all_entries(const struct chain_hash_t *ht,
                                       int32_t (*cb)(const void *key, uint32_t seed, int32_t idx, void *), void *arg) {
    if (!cb) return -1;

    int32_t total = 0;
    struct chain_hash_entry_t* entrys = get_hash_entry_list(ht);
    struct chain_hash_bucket_t* buckets = get_hash_bucket_list(ht);

    uint32_t i = 0;
    for (i = 0; i < ht->num_bucket; ++i) {
        int32_t entry_idx = buckets[i].entry_head; 
        if (entry_idx < 0) continue;

        do {
            ++total;
            const struct chain_hash_entry_t* entry = get_hash_entry(entrys, entry_idx, ht->key_len);

            #ifdef __CHAIN_HASHTABLE_FOR_IPC__
            if (cb(entry->key, entry->seed, entry->index, arg) < 0) break; 
            #else
            if (cb(entry->key, 0, entry->index, arg) < 0) break; 
            #endif

            entry_idx = entry->next;
        } while (entry_idx >= 0 && total < ht->num_entries);
    }

    return total;
}

static int32_t chain_hash_lookup_key_with_hash(struct chain_hash_t* ht, const void* key, uint32_t hash, uint32_t tm) {
    const uint32_t hash_idx = (hash & ht->hash_mask);
    struct chain_hash_entry_t* entrys = get_hash_entry_list(ht);
    struct chain_hash_bucket_t* buckets = get_hash_bucket_list(ht);

#ifdef __CHAIN_HASH_SYNC__
    __sync_synchronize();
#endif

    int32_t idx = buckets[hash_idx].entry_head;
    if (idx < 0) return -1;

    short found = 0;
    int32_t iterate_time = 0;
    struct chain_hash_entry_t* entry = 0;

    do {
        ++iterate_time;
        entry = get_hash_entry(entrys, idx, ht->key_len);

#ifndef __CHAIN_HASHTABLE_FOR_IPC__
        if (ht->cmper(key, entry->key, ht->key_len) == 0) {
            found = 1;
            break;
        }
#else
        if (memcmp(key, entry->key, ht->key_len) == 0) {
            found = 1;
            break;
        }
#endif
        idx = entry->next;
    } while (idx >= 0 && iterate_time < ht->num_entries);

    if (found) {
#ifdef __CHAIN_HASHTABLE_LRU__
        extract_from_lru_list(ht, entrys, idx);
        insert_to_lru_list(ht, entrys, idx);
#endif

#ifdef __CHAIN_HASHTABLE_CNT_LRU__
        // yep we have race here, but it doesn't really matter, benign race.
        if (tm == 0) tm = INCREMENT_LRU_TM(ht->lru_timestamp);

        entry->lru_timestamp = tm;
#endif
        return entry->index;
    }

    return -1;
}

int32_t chain_hash_lookup_key(struct chain_hash_t* ht, const void* key, uint32_t seed) {
#ifndef __CHAIN_HASHTABLE_FOR_IPC__
    uint32_t hash = ht->hasher(key, ht->key_len);
#else
    uint32_t hash = chain_hash_func_murmur_hash(seed, key, ht->key_len);
#endif

    return chain_hash_lookup_key_with_hash(ht, key, hash, 0);
}

inline __attribute__((always_inline)) void chain_hash_delete_entry(
    struct chain_hash_t *ht, struct chain_hash_entry_t *entry, int entry_idx, int bucket_idx, int prev) {

    extract_from_list(ht, bucket_idx, entry_idx, prev);

    entry->next = ht->free_list;
    ht->free_list = entry_idx;
    ht->occupied--;

#ifdef __CHAIN_HASHTABLE_CNT_LRU__
    entry->lru_timestamp = 0;
#endif

#ifdef __CHAIN_HASH_SYNC__
    __sync_synchronize();
#endif

#ifdef __CHAIN_HASHTABLE_LRU__
    struct chain_hash_entry_t* entrys = get_hash_entry_list(ht);
    extract_from_lru_list(ht, entrys, entry_idx);
#endif

#ifdef _UNIT_TEST__
    buckets[bucket_idx].num_entry--;
#endif
}

int32_t chain_hash_delete_key(struct chain_hash_t* ht, const void* key, uint32_t seed) {
#ifndef __CHAIN_HASHTABLE_FOR_IPC__
    uint32_t hash = ht->hasher(key, ht->key_len);
#else
    uint32_t hash = chain_hash_func_murmur_hash(seed, key, ht->key_len);
#endif
    struct chain_hash_entry_t* entrys = get_hash_entry_list(ht);
    struct chain_hash_bucket_t* buckets = get_hash_bucket_list(ht);

    const uint32_t hash_idx = (hash & ht->hash_mask);
    int32_t idx = buckets[hash_idx].entry_head;
    if (idx < 0) {
        chain_hash_log_error(CHET_NOT_EXIST_KEY, "delete key failed, key not exists");
        return -1;
    }

    short found = 0;
    int32_t prev = -1;
    struct chain_hash_entry_t* entry = 0;

    do {
        entry = get_hash_entry(entrys, idx, ht->key_len);

#ifndef __CHAIN_HASHTABLE_FOR_IPC__
        if (ht->cmper(key, entry->key, ht->key_len) == 0) {
            found = 1;
            break;
        }
#else
        if (memcmp(key, entry->key, ht->key_len) == 0) {
            found = 1;
            break;
        }
#endif

        prev = idx;
        idx = entry->next;
    } while (idx != -1);

    if (!found) return -1;

    chain_hash_delete_entry(ht, entry, idx, hash_idx, prev);

#ifdef __CHAIN_HASHTABLE_CNT_LRU__
    INCREMENT_LRU_TM(ht->lru_timestamp);
#endif

    return entry->index;
}

void chain_hash_reset(struct chain_hash_t* ht) {
    struct chain_hash_entry_t* entrys = get_hash_entry_list(ht);
    struct chain_hash_bucket_t* buckets = get_hash_bucket_list(ht);

    int i = 0;
    for (i = 0; i < ht->num_bucket; ++i) {
        buckets[i].entry_head = -1;
#ifdef _UNIT_TEST__
        buckets[i].num_entry = 0;
#endif
    }

#ifdef __CHAIN_HASH_SYNC__
    __sync_synchronize();
#endif

    struct chain_hash_entry_t* entry = 0;
    for (i = 0; i < ht->num_entries; ++i) {
        entry = get_hash_entry(entrys, i, ht->key_len);
        entry->index = i;
        entry->next = i + 1;
#ifdef __CHAIN_HASHTABLE_FOR_IPC__
        entry->seed = 0;
#endif

#ifdef __CHAIN_HASHTABLE_CNT_LRU__
        entry->lru_timestamp = 0;
#endif

#ifdef __CHAIN_HASHTABLE_LRU__
        entry->prev = i;
        entry->lru_next = entry->lru_prev = -1;
#endif
    }

    entry = get_hash_entry(entrys, ht->num_entries - 1, ht->key_len);
    entry->next = -1;

    ht->occupied = 0;
    ht->free_list = 0;

#ifdef __CHAIN_HASHTABLE_LRU__
    ht->lru_list_head = -1;
#endif
}

#ifdef _UNIT_TEST_
int32_t chain_hash_entry_in_bucket(const struct chain_hash_t* ht, uint32_t hash) {
    const uint32_t hash_idx = (hash & ht->hash_mask);
    struct chain_hash_bucket_t* buckets = get_hash_bucket_list(ht);
    return buckets[hash_idx].num_entry;
}
#endif

int32_t chain_hash_get_free_entry(const struct chain_hash_t* ht) {
    return ht->free_list;
}

#ifdef __CHAIN_HASHTABLE_CNT_LRU__
struct chain_hash_recycle_ret_t chain_hash_do_cnt_lru_recycle(
    struct chain_hash_t *ht, int bucket_id,
    int max, uint32_t lru_tm_threshold,
    int32_t (*cb)(uint32_t, uint32_t, void *, void*), void* arg) {

    struct chain_hash_recycle_ret_t ret = { -1, -1 };
    if (bucket_id < 0 || bucket_id >= ht->num_bucket) {
        chain_hash_log_error(CHET_INVALID_BUCKET_ID, "cnt_lru_recycle failed, invalid bucket id");
        return ret;
    }

    int total = 0;
    const uint32_t tm = ht->lru_timestamp;

    struct chain_hash_entry_t* entry = 0;
    struct chain_hash_entry_t* entrys = get_hash_entry_list(ht);

    struct chain_hash_bucket_t* bucket = 0;
    struct chain_hash_bucket_t* buckets = get_hash_bucket_list(ht);

    int32_t list_done = 0;
    uint32_t iterate_time = 0;

    while (bucket_id < ht->num_bucket && total < max) {
        list_done = 1;
        bucket = &buckets[bucket_id++];
        int entry_idx = bucket->entry_head;
        if (entry_idx < 0) continue;

        int prev = -1;

        do {
            iterate_time++;
            entry = get_hash_entry(entrys, entry_idx, ht->key_len);

            const uint32_t entry_tm = entry->lru_timestamp;

            if (entry_tm > tm || tm - entry_tm < lru_tm_threshold) {
                prev = entry_idx;
                entry_idx = entry->next;
                continue;
            }

            {
                char msg[256];
                msg[0] = 0;

                snprintf(msg, sizeof(msg), "entry expiring, bucketid:%u entryid:%u cnt:%u global cnt:%u", bucket_id - 1, entry_idx, entry_tm, tm);
                chain_hash_log_dbg(CHET_EXPIRING_ENTRY, msg);
            }

            // what if timestamp wrap around?
            ++total;
            cb(entry->index, entry->seed, entry->key, arg);

            int cur_idx = entry_idx;
            entry_idx = entry->next;
            chain_hash_delete_entry(ht, entry, cur_idx, bucket_id - 1, prev);

        } while (entry_idx != -1 && total < max && iterate_time < ht->num_entries);

        list_done = (entry_idx == -1);
    }

    ret.num_recyled = total;
    if (bucket_id == ht->num_bucket && list_done) {
        ret.cur_pos = 0;
    } else {
        ret.cur_pos = bucket_id - 1;
    }
    return ret;
}
#endif

#ifdef __CHAIN_HASHTABLE_LRU__
int chain_hash_do_lru_recycle(struct chain_hash_t* ht, int max) {
    if (ht->lru_list_head < 0) return 0;

    int i = 0;
    int idx = ht->lru_list_head;
    struct chain_hash_entry_t* entry = 0;
    struct chain_hash_entry_t* entrys = get_hash_entry_list(ht);

    while (i++ < max && ht->lru_list_head >= 0) {
        entry = get_hash_entry(entrys, ht->lru_list_head, ht->key_len);
        idx = entry->lru_prev;
        entry = get_hash_entry(entrys, idx, ht->key_len);

#ifndef __CHAIN_HASHTABLE_FOR_IPC__
        uint32_t hash = ht->hasher(entry->key, ht->key_len);
#else
        uint32_t hash = chain_hash_func_murmur_hash(entry->seed, entry->key, ht->key_len);
#endif

        const uint32_t hash_idx = (hash & ht->hash_mask);
        chain_hash_delete_entry(ht, entry, idx, hash_idx, entry->prev);
    }

    return i - 1;
}
#endif
