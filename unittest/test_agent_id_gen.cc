#include "gtest/gtest.h"
#include "agent_id_hashtable.h"

#include <unistd.h>

const uint32_t shm_key =  0x2323666;
const uint32_t shq_key =  0x4242666;
const uint32_t id_capacity = 10000;
const uint32_t req_queue_size = 4000;

struct test_item_t {
    uint32_t seed;
    uint32_t id_gen;
    char key[64];
};

int should_stop = 0;
int start_full_read = 0;
const int num_test_items = 100000;
struct test_item_t items[num_test_items];

uint32_t gen_id(uint32_t seed, const char* key, uint32_t len) {
    //http://www.isthe.com/chongo/tech/comp/fnv/
    uint32_t basic_hash = 2166136261u;
    const uint32_t fnv_prime = 16777619;

    uint8_t cur_len = 0;
    uint8_t* str = (uint8_t*)key;
    while (*str && cur_len < len) {
        basic_hash = basic_hash ^ *str;
        basic_hash = basic_hash * fnv_prime ^ seed;

        ++str;
        ++cur_len;
    }

    return basic_hash;
}

void gen_test_items() {
    srand(time(0));
    for (int i = 0; i < num_test_items; ++i) {
        items[i].seed = rand() ^ rand();
        for (int j = 0; j < 64/4; ++j) {
            uint32_t key = rand() ^ rand();
            items[i].key[j*4] = (key & 0x7f);
            items[i].key[j*4 + 1] = ((key >> 8) & 0x7f);
            items[i].key[j*4 + 2] = ((key >> 16) & 0x7f);
            items[i].key[j*4 + 3] = ((key >> 24) & 0x7f);
        }

        items[i].id_gen = gen_id(items[i].seed, items[i].key, 64);
    }
}

void* reader_thread(void* arg) {
    agent_id_hashtable_t* reader = (agent_id_hashtable_t*)arg;

    for (int i = 0; i < req_queue_size; ++i) {
        assert(get_agent_id(reader, items[i].seed, items[i].key, 64) == 0);
    }

    for (int i = 0; i < req_queue_size;) {
        uint32_t id = get_agent_id(reader, items[i].seed, items[i].key, 64);
        if (id == 0 || id == ~0) {
            usleep(100);
            continue;
        }

        assert(id == items[i].id_gen);
        ++i;
    }

    for (int i = 0; i < req_queue_size; ++i) {
        uint32_t id = get_agent_id(reader, items[i].seed, items[i].key, 64);
        assert(id == items[i].id_gen);
    }

    int total = req_queue_size;
    while (total < num_test_items) {
        for (int i = total; i < total + req_queue_size; ++i) {
            uint32_t id = get_agent_id(reader, items[i].seed, items[i].key, 64);
            assert(id == 0);
        }

        for (int i = total; i < total + req_queue_size;) {
            uint32_t id = get_agent_id(reader, items[i].seed, items[i].key, 64);

            if (id == 0 || id == ~0) {
                usleep(10);
                continue;
            }

            assert(id == items[i].id_gen);
            ++i;
        }

        for (int i = total; i < total + req_queue_size; ++i) {
            uint32_t id = get_agent_id(reader, items[i].seed, items[i].key, 64);
            assert(id == items[i].id_gen);
        }

        total += req_queue_size;
    }

    return 0;
}

void* writer_thread(void* arg) {
    uint32_t total = 0;
    agent_id_hashtable_t* writer = (agent_id_hashtable_t*)arg;
    while (!should_stop) {
        struct agent_id_req_t req[400];
        int num = get_agent_id_req(writer, req, sizeof(req)/sizeof(req[0]));
        if (num <= 0) {
            continue;
        }

        for (int i = 0; i < num; ++i) {
            uint32_t id = gen_id(req[i].seed, req[i].key_info, AGENT_ID_KEY_LEN);
            while (store_agent_id(writer, &req[i], id) < 0) {
                int c = do_ht_recycle(writer, req_queue_size/4, req_queue_size/10);
                std::cout << "do ht recycle, cur bucket:" << writer->cur_recyle_pt_ << ", num:" << c << std::endl;
            }
        }

        total += num;
        if (total % 1000 == 0) std::cout << "writer:" << total << std::endl;
    }

    assert(recycle_stray_hash_node(writer) == 0);

    return 0;
}

void* full_reader_thread(void* arg) {
    agent_id_hashtable_t* reader = (agent_id_hashtable_t*)arg;

    for (int i = 0; i < num_test_items;) {
        int r = get_agent_id(reader, items[i].seed, items[i].key, 64);
        if (r == 0 || r == ~0) {
            usleep(10);
            continue;
        }

        assert(r == items[i].id_gen);
        ++i;
        if (i % 4000 == 0) std::cout << "full reader:" << i << std::endl;
    }

    for (int i = 0; i < num_test_items; ++i) {
        int r = get_agent_id(reader, items[i].seed, items[i].key, 64);
        assert(r == items[i].id_gen);
        if (i % 4000 == 0) std::cout << "full reader2:" << i << std::endl;
    }

    return 0;
}

TEST(common_util_ut, test_agent_id_gen) {
    gen_test_items();
    clear_ipc_object(AGENT_ID_HT_WRITER, shm_key, shq_key);

    agent_id_hashtable_t writer_ht;
    assert(create_agent_id_hashtable(&writer_ht, AGENT_ID_HT_WRITER, shm_key, shq_key, id_capacity, req_queue_size) > 0);

    agent_id_hashtable_t reader_ht;
    assert(create_agent_id_hashtable(&reader_ht, AGENT_ID_HT_READER, shm_key, shq_key, id_capacity, req_queue_size) > 0);

    pthread_t reader, writer;
    pthread_attr_t attr;

    int rc = pthread_attr_init(&attr);
    ASSERT_TRUE(rc > -1);

    rc = pthread_create(&writer, NULL, writer_thread, &writer_ht);
    ASSERT_TRUE(rc > -1);

    rc = pthread_create(&reader, NULL, reader_thread, &reader_ht);
    ASSERT_TRUE(rc > -1);

    pthread_join(reader, NULL);
    should_stop = 1;
    pthread_join(writer, NULL);

    should_stop = 0;
    clear_ipc_object(AGENT_ID_HT_WRITER, shm_key, shq_key);

    agent_id_hashtable_t full_writer_ht;
    assert(create_agent_id_hashtable(&full_writer_ht, AGENT_ID_HT_WRITER, shm_key, shq_key, num_test_items, req_queue_size) > 0);

    agent_id_hashtable_t full_reader_ht1;
    assert(create_agent_id_hashtable(&full_reader_ht1, AGENT_ID_HT_READER, shm_key, shq_key, num_test_items, req_queue_size) > 0);
    agent_id_hashtable_t full_reader_ht2;
    assert(create_agent_id_hashtable(&full_reader_ht2, AGENT_ID_HT_READER, shm_key, shq_key, num_test_items, req_queue_size) > 0);

    pthread_t fw, fr1, fr2;
    rc = pthread_create(&fw, NULL, writer_thread, &full_writer_ht);
    ASSERT_TRUE(rc > -1);

    rc = pthread_create(&fr1, NULL, full_reader_thread, &full_reader_ht1);
    ASSERT_TRUE(rc > -1);
    rc = pthread_create(&fr2, NULL, full_reader_thread, &full_reader_ht2);
    ASSERT_TRUE(rc > -1);

    pthread_join(fr1, NULL);
    pthread_join(fr2, NULL);
    should_stop = 1;
    pthread_join(fw, NULL);
}

