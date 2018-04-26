#ifndef __AGENT_ID_HASHTABLE_H__ 
#define __AGENT_ID_HASHTABLE_H__

#include <stdint.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include <sys/types.h>

#define AGENT_ID_KEY_LEN (64)

struct chain_hash_t;

enum agent_id_ht_type_t {
    AGENT_ID_HT_NONE = 0,
    AGENT_ID_HT_READER = 1,
    AGENT_ID_HT_WRITER = 2,
};

enum agent_id_gen_err_t {
    AIGET_NO_ERR = 0,
    AIGET_SENDQ_FAIL = 1,
    AIGET_ADD_ID_FAIL = 2,
    AIGET_RECV_FAIL = 3,
};

struct agent_id_hashtable_t {
    uint32_t type_;
    uint32_t capacity_;
    uint32_t q_capacity_;
    uint32_t total_mem_size_;
    key_t shm_key_;
    key_t shq_key_;
    int32_t last_errno_;
    int32_t req_queue_id_;
    int32_t cur_recyle_pt_;
    struct chain_hash_t* ht_;
    uint32_t* id_cache_;
};

#pragma pack(1)
struct agent_id_req_t {
    uint32_t seed;
    char key_info[AGENT_ID_KEY_LEN];
};
#pragma pack(0)

#ifdef __cplusplus
extern "C" {
#endif

int32_t clear_ipc_object(int type, int shm_key, int shq_key);
uint32_t get_agent_id(struct agent_id_hashtable_t* ht, uint32_t seed, const char* key, int len);
uint32_t get_agent_id_req(struct agent_id_hashtable_t* ht, struct agent_id_req_t* arr, int items);
int32_t store_agent_id(struct agent_id_hashtable_t* ht, struct agent_id_req_t* req, uint32_t agent_id);

int32_t recycle_stray_hash_node(struct agent_id_hashtable_t* ht);
int32_t do_ht_recycle(struct agent_id_hashtable_t* ht, int threshold, int max);
int32_t create_agent_id_hashtable(struct agent_id_hashtable_t* ht, int type, key_t shm_key, key_t shq_key, uint32_t m_capacity, uint32_t q_capacity);

#ifdef __cplusplus
}
#endif

#endif
