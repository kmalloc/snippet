#ifndef __CHAIN_HASH_H__
#define __CHAIN_HASH_H__

#include <stdint.h>
#include <stdlib.h>

typedef uint32_t (*chain_hash_hash_func_t)(const void* key, uint32_t len);
typedef void* (*chain_hash_copy_func_t)(void* dst, const void* src, size_t len);
typedef int32_t (*chain_hash_key_cmp_func_t)(const void* k1, const void* k2, size_t len);

enum chain_hash_err_t {
    CHET_NONE,
    CHET_BAD_COOKIE,
    CHET_BAD_TABLE,
    CHET_BAD_TABLE_SIZE,

    CHET_NO_FREE_ENTRY,
    CHET_NOT_EXIST_KEY,
    CHET_EXPIRING_ENTRY,
    CHET_INVALID_BUCKET_ID,
    CHET_LOOP_IN_ENTRY_LIST,
};

#ifdef __CHAIN_HASHTABLE_FOR_IPC__
#pragma pack(1)
#endif
struct chain_hash_t {
#ifdef __CHAIN_HASHTABLE_FOR_IPC__
    char cookie_prefix[8];
#endif

#ifdef __CHAIN_HASHTABLE_CNT_LRU__
    // make sure following is 4 bytes aligned.
    // lookup & delete_key will increment this only when succed.
    uint32_t lru_timestamp;
#endif

    uint32_t num_bucket;
    uint32_t num_entries;

    uint32_t key_len;
    uint32_t occupied;
    uint32_t hash_mask;

    int32_t free_list;

    uint32_t entry_list_off_;
    uint32_t bucket_list_off_;

#ifdef __CHAIN_HASHTABLE_LRU__
    int32_t lru_list_head;
#endif

#ifndef __CHAIN_HASHTABLE_FOR_IPC__
    chain_hash_hash_func_t hasher;
    chain_hash_copy_func_t copyer;
    chain_hash_key_cmp_func_t cmper;
#else
    char cookie_postfix[8];
#endif
};

#ifdef __CHAIN_HASHTABLE_FOR_IPC__
#pragma pack(0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// user need to implement following functions.
void chain_hash_mem_free(void* p);
void* chain_hash_mem_alloc(uint32_t sz);

void chain_hash_reset(struct chain_hash_t*);
void chain_hash_destroy(struct chain_hash_t*);

uint32_t chain_hash_calc_mem(uint32_t num_bucket, uint32_t num_entries, uint32_t key_len);
struct chain_hash_t* chain_hash_create(uint32_t num_bucket, uint32_t num_entries, uint32_t key_len);
struct chain_hash_t* chain_hash_create_with_mem(uint32_t num_bucket, uint32_t num_entries, uint32_t key_len, void* mem, uint32_t len);

int32_t chain_hash_get_free_entry(const struct chain_hash_t* ht);
int32_t chain_hash_add_key(struct chain_hash_t* ht, const void* key, uint32_t seed);

int32_t chain_hash_lookup_key(struct chain_hash_t* ht, const void* key, uint32_t seed);
int32_t chain_hash_delete_key(struct chain_hash_t* ht, const void* key, uint32_t seed);

int32_t chain_hash_iterate_all_entries(const struct chain_hash_t* ht, int32_t (*cb)(const void* key, uint32_t seed, int32_t idx, void* arg), void* arg);

void chain_hash_log_dbg(int type, const char* msg);
void chain_hash_log_error(int type, const char* msg);

#ifdef _UNIT_TEST_
int32_t chain_hash_entry_in_bucket(const struct chain_hash_t* ht, uint32_t hash);
#endif

#ifdef __CHAIN_HASHTABLE_LRU__
int chain_hash_do_lru_recycle(struct chain_hash_t* ht, int max);
#endif

#ifdef __CHAIN_HASHTABLE_FOR_IPC__
int32_t chain_hash_recycle_stray_entry(struct chain_hash_t* ht);
struct chain_hash_t* chain_hash_attach_mem(void* mem, uint32_t len);
#endif

#ifdef __CHAIN_HASHTABLE_CNT_LRU__
struct chain_hash_recycle_ret_t {
    int32_t cur_pos;
    int32_t num_recyled;
};
struct chain_hash_recycle_ret_t chain_hash_do_cnt_lru_recycle(
    struct chain_hash_t* ht, int bucket_id,
     int max, uint32_t lru_tm_threshold, int32_t (*cb)(uint32_t, uint32_t, void*, void*), void* arg); // index, seed, key
#endif

#ifdef __cplusplus
}
#endif

#endif
