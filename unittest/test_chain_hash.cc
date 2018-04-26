#include "gtest/gtest.h"

#include "chain_hash.h"
#include "MurMurHash3.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void chain_hash_mem_free(void* p) {
    free(p);
}

void* chain_hash_mem_alloc(uint32_t size) {
    return malloc(size);
}

#ifndef __CHAIN_HASHTABLE_FOR_IPC__
TEST(common_util_ut, test_chain_hash) {
#define num_bit 24
    struct chain_hash_t* ht = chain_hash_create(1 << num_bit, 1 << num_bit, 4);
    static uint32_t s_d[2048];

    int cur = 0;

    // 64 entries
    for (int i = 0; i < 64; ++i) {
        s_d[cur++] = i;
    }

    uint32_t occupied = cur;

    // 128 entries
    for (uint32_t i = 0; i < 64;) {
        s_d[cur++] = (occupied + i) | (i << num_bit);
        s_d[cur++] = (occupied + i) | ((i+1) << num_bit);

        i+=2;
    }

    occupied = cur;

    // 192 entries
    for (uint32_t i = 0; i < 64;) {
        s_d[cur++] = (occupied + i) | (i << num_bit);
        s_d[cur++] = (occupied + i) | ((i+1) << num_bit);
        s_d[cur++] = (occupied + i) | ((i+2) << num_bit);

        i+=3;
    }

    occupied = cur;

    // 256 entries
    for (uint32_t i = 0; i < 64;) {
        s_d[cur++] = (occupied + i) | (i << num_bit);
        s_d[cur++] = (occupied + i) | ((i+1) << num_bit);
        s_d[cur++] = (occupied + i) | ((i+2) << num_bit);
        s_d[cur++] = (occupied + i) | ((i+3) << num_bit);

        i+=4;
    }

    occupied = cur;

    // 300 entries
    for (uint32_t i = 0; i < 256; ++i) {
        s_d[cur++] = (occupied) | (i << num_bit);
    }

    static char s_d_r[1 << num_bit];
    for (int i = 0; i < sizeof(s_d_r); ++i) s_d_r[i] = -1;

    for (int i = 0; i < cur; ++i) {
        int32_t r = chain_hash_add_key(ht, &s_d[i], 0);

        assert(r >= 0);
        assert(r < (1 << num_bit));

        assert(s_d_r[r] == -1);

        s_d_r[r] = 1;
    }

    assert(ht->occupied == cur);

    for (int i = 0; i < cur; ++i) {
        int32_t r = chain_hash_lookup_key(ht, &s_d[i], 0);

        assert(r >= 0);
        assert(r < (1 << num_bit));

        assert(s_d_r[r] == 1);

        s_d_r[r]++;
    }

    for (int i = 0; i < 64; ++i) {
        assert(1 == chain_hash_entry_in_bucket(ht, i));
    }

    occupied = 64;

    for (uint32_t i = 0; i < 64/2; ++i) {
        int num = chain_hash_entry_in_bucket(ht, occupied + 2 * i);
        assert(2 == num);
    }

    occupied += 64;

    // 192 entries
    for (uint32_t i = 0; i < 64/3; i++) {
        assert(3 == chain_hash_entry_in_bucket(ht, occupied + 3 * i));
    }

    occupied += 66;

    // 256 entries
    for (uint32_t i = 0; i < 64/4; ++i) {
        assert(4 == chain_hash_entry_in_bucket(ht, occupied + 4 * i));
    }

    occupied += 64;

    assert(256 == chain_hash_entry_in_bucket(ht, occupied));

    // test deletion
    for (int i = 0; i < 64; i++) {
        if (i < 32) {
            int r = chain_hash_delete_key(ht, &s_d[i], 0);

            ASSERT_TRUE(r >= 0);
            ASSERT_TRUE(r < (1 << num_bit));
            ASSERT_TRUE(0 > chain_hash_lookup_key(ht, &s_d[i], 0));
        } else {
            assert(chain_hash_lookup_key(ht, &s_d[i], 0) >= 0);
        }
    }

    occupied = 64;

    for (uint32_t i = 0; i < 64; i += 2) {
        if (i < 32) {
            int r = chain_hash_delete_key(ht, &s_d[occupied + i + 1], 0);

            assert(r >= 0);
            assert(r < (1 << num_bit));
            assert(0 > chain_hash_lookup_key(ht, &s_d[occupied + i + 1], 0));

            r = chain_hash_delete_key(ht, &s_d[occupied + i], 0);

            ASSERT_TRUE(r >= 0) << " r:" << r;
            assert(r < (1 << num_bit));
            assert(0 > chain_hash_lookup_key(ht, &s_d[occupied + i], 0));

            assert(chain_hash_lookup_key(ht, &s_d[occupied + i + 32], 0) >= 0);
            assert(chain_hash_lookup_key(ht, &s_d[occupied + i + 32 + 1], 0) >= 0);
        } else {
            assert(chain_hash_lookup_key(ht, &s_d[occupied + i], 0) >= 0);
            assert(chain_hash_lookup_key(ht, &s_d[occupied + i + 1], 0) >= 0);
        }
    }

    occupied += 64;

    for (uint32_t i = 0; i < 64; i += 3) {
        if (i < 32) {
            int r = chain_hash_delete_key(ht, &s_d[occupied + i + 1], 0);

            assert(r >= 0);
            assert(r < (1 << num_bit));
            assert(0 > chain_hash_lookup_key(ht, &s_d[occupied + i + 1], 0));

            r = chain_hash_delete_key(ht, &s_d[occupied + i + 2], 0);

            assert(r >= 0);
            assert(r < (1 << num_bit));
            assert(0 > chain_hash_lookup_key(ht, &s_d[occupied + i + 2], 0));

            r = chain_hash_delete_key(ht, &s_d[occupied + i], 0);

            assert(r >= 0);
            assert(r < (1 << num_bit));
            assert(0 > chain_hash_lookup_key(ht, &s_d[occupied + i], 0));

            assert(chain_hash_lookup_key(ht, &s_d[occupied + 33 + i], 0) >= 0);
            assert(chain_hash_lookup_key(ht, &s_d[occupied + 33 + i + 1], 0) >= 0);
            assert(chain_hash_lookup_key(ht, &s_d[occupied + 33 + i + 2], 0) >= 0);
        } else {
            assert(chain_hash_lookup_key(ht, &s_d[occupied + i], 0) >= 0);
            assert(chain_hash_lookup_key(ht, &s_d[occupied + i + 1], 0) >= 0);
            assert(chain_hash_lookup_key(ht, &s_d[occupied + i + 2], 0) >= 0);
        }
    }

    occupied += 66;
    int r = chain_hash_delete_key(ht, &s_d[occupied + 255], 0);
    assert(r >= 0);
    assert(r < (1 << num_bit));
    assert(chain_hash_lookup_key(ht, &s_d[occupied + 255], 0) < 0);

    r = chain_hash_delete_key(ht, &s_d[occupied], 0);
    assert(r >= 0);
    assert(r < (1 << num_bit));
    assert(chain_hash_lookup_key(ht, &s_d[occupied], 0) < 0);

    r = chain_hash_delete_key(ht, &s_d[occupied + 100], 0);
    assert(r >= 0);
    assert(r < (1 << num_bit));
    assert(chain_hash_lookup_key(ht, &s_d[occupied + 100], 0) < 0);

    r = chain_hash_delete_key(ht, &s_d[occupied + 110], 0);
    assert(r >= 0);
    assert(r < (1 << num_bit));
    assert(chain_hash_lookup_key(ht, &s_d[occupied + 110], 0) < 0);

    assert(chain_hash_lookup_key(ht, &s_d[occupied + 144], 0) >= 0);
    assert(chain_hash_lookup_key(ht, &s_d[occupied + 148], 0) >= 0);
    assert(chain_hash_lookup_key(ht, &s_d[occupied + 145], 0) >= 0);

    chain_hash_reset(ht);

    assert(ht->key_len == 4);
    assert(ht->occupied == 0);
    assert(ht->free_list == 0);

    chain_hash_destroy(ht);

    printf("test chain hash done!!!\n");
}
#else
TEST(common_util_ut, test_chain_hash_ipc_impl) {
}
#endif

void chain_hash_log_error(int type, const char* msg) {
    std::cout << "chain hash error msg, type:" << type << ", msg:" << msg << std::endl;
}

void chain_hash_log_dbg(int type, const char* msg) {
    std::cout << "chain hash error msg, type:" << type << ", msg:" << msg << std::endl;
}

#ifdef __CHAIN_HASHTABLE_CNT_LRU__
static int32_t recycle_cb(uint32_t index, uint32_t seed, void* key, void* arg) {
    return 0;
}

TEST(common_util_ut, test_chain_hash_cnt_lru) {
    const uint32_t seed = 0x23ef43;
    const uint32_t bucket_num_max = 8, item_num_max = 32;
    struct chain_hash_t* ht = chain_hash_create(bucket_num_max, item_num_max, 8);

    char* keys[] = {"abcd6e2f", "Abc2de2f", "ww20wwww", "vvvg2223", "swjdlkss", "kd29fghs",
        "djswefvv", "gg2dfwjf", "sdkfwef0", "dkfjswvv", "sdlkfjsl", "3lkelf0", "02jd2j4h",
        "lsdkjf2h", "wjfv02hj", "2hf20jvv", "2dkwivjw", "2hshsv9s", "2hfwgh0w", "wh02jfhs", "vjwkkj02",
        "nhw;owjd", "wh092jfs", "whfw293g", "hwowhdsg", "wghhjfwh", "wjfhslww", "gwhwhwlh"};
    const size_t num_item = sizeof(keys)/sizeof(keys[0]);

    for (size_t i = 0; i < num_item; ++i) {
        ASSERT_EQ(i, chain_hash_add_key(ht, keys[i], seed));
    }

    for (size_t i = 0; i < num_item; ++i) {
        ASSERT_EQ(i, chain_hash_lookup_key(ht, keys[i], seed));
    }

    for (int i = num_item - 1; i >= 0; --i) {
        ASSERT_EQ(i, chain_hash_delete_key(ht, keys[i], seed));
    }

    for (size_t i = 0; i < num_item/2; ++i) {
        ASSERT_EQ(i, chain_hash_add_key(ht, keys[i], seed));
    }

    for (size_t i = 0; i < num_item; ++i) {
        if (i < num_item/2) {
            ASSERT_EQ(i, chain_hash_lookup_key(ht, keys[i], seed));
        } else {
            ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[i], seed));
        }
    }

    for (int i = 0; i < num_item; ++i) {
        if (i < num_item/2) {
            ASSERT_EQ(i, chain_hash_delete_key(ht, keys[i], seed));
        } else {
            ASSERT_LT(chain_hash_delete_key(ht, keys[i], seed), 0);
        }
    }

    int global_tm = 0;
    ht->lru_timestamp = 0;

    for (size_t i = 0; i < num_item; ++i) {
        global_tm++;
        if (i < num_item/2) {
            ASSERT_EQ(num_item/2 - i - 1, chain_hash_add_key(ht, keys[i], seed)) << "num_item/2:" << num_item/2 << ", at index:" << i;
        } else {
            ASSERT_EQ(i, chain_hash_add_key(ht, keys[i], seed));
        }
    }

    ASSERT_EQ(num_item, global_tm);
    ASSERT_EQ(num_item, ht->occupied);
    ASSERT_EQ(num_item, ht->lru_timestamp);

    global_tm++;
    ASSERT_EQ(num_item/2 - 1, chain_hash_lookup_key(ht, keys[0], seed));

    ASSERT_EQ(global_tm, ht->lru_timestamp);

    int total = 0;
    chain_hash_recycle_ret_t ret;
    ret = chain_hash_do_cnt_lru_recycle(ht, 0, 3, num_item - 1, recycle_cb, 0);
    ASSERT_GE(ret.num_recyled, 0);
    total += ret.num_recyled;

    ret = chain_hash_do_cnt_lru_recycle(ht, 2, 3, num_item - 1, recycle_cb, 0);
    ASSERT_GE(ret.num_recyled, 0);
    total += ret.num_recyled;

    ASSERT_EQ(1, total);
    // global_tm++;
    ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[1], seed));

    // global_tm:8
    for (size_t i = 0; i < num_item; ++i) {
        if (i == 1) {
            ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[i], seed));
            continue;
        } else if (i < num_item/2) {
            ASSERT_EQ(num_item/2 - i - 1, chain_hash_lookup_key(ht, keys[i], seed));
        } else {
            ASSERT_EQ(i, chain_hash_lookup_key(ht, keys[i], seed));
        }

        global_tm++;
    }

    // 0:9 2:10 3:11 4:12 5:13 6:14

    global_tm++;
    ASSERT_EQ(num_item/2 - 1, chain_hash_delete_key(ht, keys[0], seed));
    ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[0], seed));

    // global_tm:15
    ASSERT_EQ(num_item - 2, ht->occupied);
    ASSERT_EQ(global_tm, ht->lru_timestamp);

    ret = chain_hash_do_cnt_lru_recycle(ht, 0, 3, num_item - 3, recycle_cb, 0);
    ASSERT_EQ(2, ret.num_recyled) << "global tm:" << global_tm;
    ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[2], seed));
    ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[3], seed));

    for (size_t i = 0; i < num_item; ++i) {
        if (i < 4) {
            ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[i], seed));
            continue;
        } else if (i < num_item/2) {
            ASSERT_EQ(num_item/2 - i - 1, chain_hash_lookup_key(ht, keys[i], seed));
        } else {
            ASSERT_EQ(i, chain_hash_lookup_key(ht, keys[i], seed));
        }
        global_tm++;
    }

    //global_tm:19
    for (size_t i = 0; i < 4; ++i) {
        ASSERT_TRUE(chain_hash_add_key(ht, keys[i], seed) >= 0);
    }
    ASSERT_EQ(num_item, ht->occupied);

    std::map<int, int> pos;
    for (size_t i = 0; i < num_item; ++i) {
        int hash, hash_id;
        MurmurHash3_x86_32(keys[i], ht->key_len, seed, &hash);

        hash_id = (hash&ht->hash_mask);
        pos[hash_id]++;
        std::cout << "i:" << i << ", hashid:" << hash_id << std::endl;
    }

    // global_tm:22
    // 0:20 1:21 2:22 3:16 4:17 5:18 6:19

    int half = 0, half_bucket = 0;
    std::map<int, int>::iterator it;
    for (it = pos.begin(); it != pos.end() && half < num_item/2; ++it) {
        half_bucket = it->first;
        half += it->second;
    }

    std::cout << "half bucket id:" << half_bucket << ", num entry:" << half << std::endl;

    ret = chain_hash_do_cnt_lru_recycle(ht, half_bucket + 1, num_item, 0, recycle_cb, 0);
    ASSERT_EQ(num_item - half, ret.num_recyled);

    for (size_t i = 0; i < num_item; ++i) {
        ASSERT_TRUE(chain_hash_add_key(ht, keys[i], seed) >= 0);
    }

    const int run_lookup_cnt = 100;
    for (int c = 0; c < run_lookup_cnt; c++) {
        for (size_t i = num_item/4; i < 3*num_item/4; ++i) {
            int r = chain_hash_lookup_key(ht, keys[i], seed);
            ASSERT_TRUE(r >= 0) << "ret:" << r << ", c:" << c << ", i:" << i << ", key:" << keys[i];
        }
    }

    const int item_updated = (3*num_item/4 - num_item/4);
    const int item_outdated = num_item - item_updated;
    const int gap = run_lookup_cnt * item_updated;

    std::cout << "total item:" << num_item << ", total outdated:" << item_outdated << std::endl;

    ret = chain_hash_do_cnt_lru_recycle(ht, 0, item_outdated/2, gap, recycle_cb, 0);
    ASSERT_EQ(item_outdated/2, ret.num_recyled);
    ret = chain_hash_do_cnt_lru_recycle(ht, 0, item_outdated, gap, recycle_cb, 0);
    ASSERT_EQ(item_outdated - item_outdated/2, ret.num_recyled);

    for (size_t i = 0; i < num_item; ++i) {
        ASSERT_TRUE(chain_hash_add_key(ht, keys[i], seed) >= 0);
    }

    for (int c = 0; c < run_lookup_cnt; c++) {
        for (size_t i = num_item/4; i < 3*num_item/4; ++i) {
            ASSERT_TRUE(chain_hash_lookup_key(ht, keys[i], seed) >= 0);
        }
    }

    ret = chain_hash_do_cnt_lru_recycle(ht, 0, item_outdated/4, gap, recycle_cb, 0);
    ASSERT_EQ(item_outdated/4, ret.num_recyled);

    std::cout << "done recycling 1/4, num::" << item_outdated/4 << std::endl;

    total = 0;
    while (ret.cur_pos >= 0 && total < item_outdated - item_outdated/4) {
        ret = chain_hash_do_cnt_lru_recycle(ht, ret.cur_pos, item_outdated / 8, gap, recycle_cb, 0);
        total += ret.num_recyled;
        std::cout << "done recycling 1/8, num:" << ret.num_recyled << ", total:" << total << std::endl;
    }

    ASSERT_EQ(item_outdated - item_outdated/4, total);
}
#endif

#ifdef __CHAIN_HASHTABLE_LRU__
TEST(common_util_ut, test_chain_hash_lru) {
    const uint32_t bucket_num_max = 4, item_num_max = 9;
    struct chain_hash_t* ht = chain_hash_create(bucket_num_max, item_num_max, 6);

    char* keys[] = {"abcdef", "Abcdef", "wwwwww", "vvvg23", "sdlkss", "djswef", "2dfwjf"};
    const size_t num_item = sizeof(keys)/sizeof(keys[0]);
    for (size_t i = 0; i < num_item; ++i) {
        ASSERT_EQ(i, chain_hash_add_key(ht, keys[i], 0));
    }

    ASSERT_EQ(num_item, ht->occupied);

    // lru list: 6 5 4 3 2 1 0

    ASSERT_EQ(1, chain_hash_do_lru_recycle(ht, 1));
    ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[0], 0));
    // after recycle: 6 5 4 3 2 1

    for (size_t i = 1; i < num_item; ++i) {
        ASSERT_EQ(i, chain_hash_lookup_key(ht, keys[i], 0));
    }
    // after lookup: 6 5 4 3 2 1

    ASSERT_EQ(1, chain_hash_lookup_key(ht, keys[1], 0));
    // after lookup: 1 6 5 4 3 2

    ASSERT_EQ(2, chain_hash_do_lru_recycle(ht, 2));
    // after lookup: 1 6 5 4
    // free list: 3 2 0

    ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[2], 0));
    ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[3], 0));
    for (size_t i = 0; i < num_item; ++i) {
        if (i == 0 || i == 2 || i == 3) {
            ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[i], 0));
        } else {
            ASSERT_EQ(i, chain_hash_lookup_key(ht, keys[i], 0));
        }
    }
    // lru: 6 5 4 1

    // free list 3 2 0
    ASSERT_EQ(3, chain_hash_add_key(ht, keys[0], 0));
    // lru: 0 6 5 4 1

    ASSERT_EQ(1, chain_hash_do_lru_recycle(ht, 1));
    // lru: 0 6 5 4
    // free list: 1 2 0
    ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[1], 0));
    ASSERT_EQ(3, chain_hash_lookup_key(ht, keys[0], 0));

    // free list 1 2 0
    ASSERT_EQ(1, chain_hash_add_key(ht, keys[1], 0));
    ASSERT_EQ(2, chain_hash_add_key(ht, keys[2], 0));
    ASSERT_EQ(0, chain_hash_add_key(ht, keys[3], 0));

    // lru: 3 2 1 0 6 5 4
    for (size_t i = 0; i < 4; ++i) {
        ASSERT_TRUE(chain_hash_lookup_key(ht, keys[i], 0) >= 0);
    }
    for (int i = 3; i >= 0; --i) {
        ASSERT_TRUE(chain_hash_lookup_key(ht, keys[i], 0) >= 0);
    }

    ASSERT_EQ(3, chain_hash_do_lru_recycle(ht, 3));
    ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[4], 0));
    ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[5], 0));
    ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[6], 0));
    for (size_t i = 0; i < 4; ++i) {
        ASSERT_TRUE(chain_hash_lookup_key(ht, keys[i], 0) >= 0);
    }

    ASSERT_EQ(4, chain_hash_do_lru_recycle(ht, 4));
    ASSERT_EQ(0, ht->occupied);
    for (size_t i = 0; i < num_item; ++i) {
        ASSERT_EQ(-1, chain_hash_lookup_key(ht, keys[i], 0));
    }
}
#endif

TEST(common_util_ut, test_chain_over_add) {
    const uint32_t bucket_num_max = 17, item_num_max = 9;
    struct chain_hash_t* ht = chain_hash_create(bucket_num_max, item_num_max, 4);

    assert(32 == ht->num_bucket);
    assert(9 == ht->num_entries);

    uint32_t key = 0;
    for (int i = 0; i < item_num_max; ++i, ++key) {
        assert(i == chain_hash_add_key(ht, &key, 0));
        assert(i == chain_hash_add_key(ht, &key, 0));
    }

    assert(-1 == ht->free_list);
    assert(item_num_max == ht->occupied);

    assert(-1 == chain_hash_add_key(ht, &key, 0));

    ++key;
    assert(-1 == chain_hash_add_key(ht, &key, 0));

    key = item_num_max - 1;
    assert(item_num_max - 1 == chain_hash_delete_key(ht, &key, 0));

    assert(item_num_max - 1 == ht->occupied);
    assert(item_num_max - 1 == ht->free_list);

    key = 27;
    assert(item_num_max - 1 == chain_hash_add_key(ht, &key, 0));

    assert(-1 == ht->free_list);

    assert(item_num_max == ht->occupied);

    key++;
    assert(-1 == chain_hash_add_key(ht, &key, 0));

    assert(item_num_max == ht->occupied);
    assert(-1 == ht->free_list);

    key = 2;
    assert(2 == chain_hash_delete_key(ht, &key, 0));
    assert(item_num_max - 1 == ht->occupied);
    assert(2 == ht->free_list);

    key = 3;
    assert(3 == chain_hash_delete_key(ht, &key, 0));
    assert(item_num_max - 2 == ht->occupied);
    assert(3 == ht->free_list);

    chain_hash_destroy(ht);
    printf("test chain hash over add done!!!\n");
}

