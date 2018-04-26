#include "agent_id_hashtable.h"

#include "chain_hash.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define AGENT_ID_HASH_FACTOR (1.8)

struct msg_type_t {
    long mtype; // for internal
    struct agent_id_req_t req;
};

uint32_t get_agent_id(struct agent_id_hashtable_t* ht, uint32_t seed, const char* key, int len) {
    int32_t idx = chain_hash_lookup_key(ht->ht_, key, seed);
    if (idx >= 0 && idx < ht->capacity_) return ht->id_cache_[idx];

    struct msg_type_t req;
    req.mtype = 1; // must > 0, required by API.
    req.req.seed = seed;

    memcpy(req.req.key_info, key, AGENT_ID_KEY_LEN);

    int32_t ret = msgsnd(ht->req_queue_id_, &req, sizeof(struct msg_type_t) - sizeof(long), IPC_NOWAIT);
    if (ret < 0) {
        // printf("sendq error:%d\n", errno);
        ht->last_errno_ = AIGET_SENDQ_FAIL;
        return ~0u;
    }

    return 0;
}

int32_t store_agent_id(struct agent_id_hashtable_t* ht, struct agent_id_req_t* req, uint32_t agent_id) {
    if (ht->type_ != AGENT_ID_HT_WRITER) return -1;

    int32_t idx = chain_hash_add_key(ht->ht_, req->key_info, req->seed);
    if (idx < 0 || idx >= ht->capacity_) {
        ht->last_errno_ = AIGET_ADD_ID_FAIL;
        return -2;
    }

    ht->id_cache_[idx] = agent_id;
    return idx;
}

uint32_t get_agent_id_req(struct agent_id_hashtable_t* ht, struct agent_id_req_t* arr, int items) {
    if (items <= 0 || ht->type_ != AGENT_ID_HT_WRITER) return 0;

    uint32_t i = 0;
    struct msg_type_t msg;

    ht->last_errno_ = AIGET_NO_ERR;

    while (i < items) {
        int32_t len = msgrcv(ht->req_queue_id_, &msg, sizeof(struct msg_type_t) - sizeof(long), 0, MSG_NOERROR|IPC_NOWAIT);
        if (len == -1 && (EINTR == errno || E2BIG == errno)) continue;

        if (len < 0) {
            ht->last_errno_ = AIGET_RECV_FAIL;
            break;
        }

        arr[i] = msg.req;

        ++i;
    }

    return i;
}

static int32_t recycle_cb(uint32_t index, uint32_t seed, void* key, void* arg) {
    struct agent_id_hashtable_t *ht = (struct agent_id_hashtable_t *)arg;
    if (index >= ht->capacity_) return -1;

    ht->id_cache_[index] = 0;
    return 0;
}

int32_t do_ht_recycle(struct agent_id_hashtable_t* ht, int threshold, int max) {
    if (ht->ht_->occupied < threshold) return 0;

    int i = 2;
    struct chain_hash_recycle_ret_t ret =
        chain_hash_do_cnt_lru_recycle(ht->ht_, ht->cur_recyle_pt_, max, ht->ht_->lru_timestamp/i, recycle_cb, ht);
    while (ret.num_recyled <= 0 && ht->ht_->lru_timestamp/i > 0) {
        ++i;
        ret = chain_hash_do_cnt_lru_recycle(ht->ht_, ht->cur_recyle_pt_, max, ht->ht_->lru_timestamp/i, recycle_cb, ht);
    }

    ht->cur_recyle_pt_ = ret.cur_pos;

    return ret.num_recyled;
}

int32_t create_agent_id_hashtable(struct agent_id_hashtable_t* ht, int type,
                                  key_t shm_key, key_t shq_key, uint32_t m_capacity, uint32_t q_capacity) {
    if (type != AGENT_ID_HT_READER && type != AGENT_ID_HT_WRITER) return -8;

    char* shm = 0;
    int shm_id = 0;

    ht->type_ = type;
    ht->shm_key_ = shm_key;
    ht->shq_key_ = shq_key;
    ht->cur_recyle_pt_ = 0;
    ht->total_mem_size_ = 0;

    uint32_t arr_sz = 0;
    uint32_t ht_size = 0;
    uint32_t num_bucket = 0;
    int32_t q_flags = 0644, m_flags = 0644;
    unsigned char is_writer = (type == AGENT_ID_HT_WRITER);

    if ((shm_id = shmget(shm_key, ht->total_mem_size_, m_flags)) < 0 && is_writer) {
        m_flags |= IPC_CREAT;
        arr_sz = sizeof(uint32_t) * m_capacity;
        num_bucket = m_capacity * AGENT_ID_HASH_FACTOR;
        ht_size = chain_hash_calc_mem(num_bucket, m_capacity, AGENT_ID_KEY_LEN);

        ht->capacity_ = m_capacity;
        ht->total_mem_size_ = ht_size + arr_sz;
        shm_id = shmget(shm_key, ht->total_mem_size_, m_flags);
    } 

    if (shm_id < 0 || (shm = shmat(shm_id, NULL, 0)) == (char *)-1) return -2;

    if (m_flags & IPC_CREAT) {
        ht->ht_ = chain_hash_create_with_mem(num_bucket, m_capacity, AGENT_ID_KEY_LEN, shm, ht->total_mem_size_);
        if (!ht->ht_) {
            shmdt(shm);
            return -7;
        }
    } else {
        struct shmid_ds md;
        if (shmctl(shm_id, IPC_STAT, &md) < 0) return -9;

        ht->ht_ = chain_hash_attach_mem(shm, md.shm_segsz);
        if (!ht->ht_) {
            shmdt(shm);
            return -7;
        }

        ht->capacity_ = m_capacity = ht->ht_->num_entries;
        arr_sz = sizeof(uint32_t) * m_capacity;
        num_bucket = m_capacity * AGENT_ID_HASH_FACTOR;
        ht_size = chain_hash_calc_mem(num_bucket, m_capacity, AGENT_ID_KEY_LEN);
        ht->total_mem_size_ = ht_size + arr_sz;

        if (ht->total_mem_size_ > md.shm_segsz) return -10;
    }

    ht->id_cache_ = (uint32_t*)(shm + ht_size);

    if (ht->ht_->num_entries != m_capacity ||
        ht->ht_->key_len != AGENT_ID_KEY_LEN ||
        ht->ht_->num_bucket < m_capacity * AGENT_ID_HASH_FACTOR) {
        shmdt(shm);
        return -3;
    }

    ht->req_queue_id_ = msgget(shq_key, q_flags);
    if (ht->req_queue_id_ == -1 && is_writer) {
        q_flags |= IPC_CREAT;
        ht->req_queue_id_ = msgget(shq_key, q_flags);
    }

    if (ht->req_queue_id_ < 0) {
        shmdt(shm);
        return -4;
    }

    struct msqid_ds qd;
    if (msgctl(ht->req_queue_id_, IPC_STAT, &qd) < 0) {
        shmdt(shm);
        ht->req_queue_id_ = -1;
        return -5;
    }

    if (q_flags & IPC_CREAT) {
        ht->q_capacity_ = q_capacity;
        qd.msg_qbytes = sizeof(struct msg_type_t) * q_capacity;
        if (msgctl(ht->req_queue_id_, IPC_SET, &qd) < 0) {
            shmdt(shm);
            ht->req_queue_id_ = -1;
            return -6;
        }
    } else {
        ht->q_capacity_ = qd.msg_qnum;
    }

    return 1;
}

int32_t clear_ipc_object(int type, key_t shm_key, key_t shq_key) {
    if (type != AGENT_ID_HT_WRITER) return -1;

    int32_t ret1 = -1, ret2 = -1;
    int shm_id = shmget(shm_key, 0, 0600);
    if (shm_id > 0) {
        ret1 = shmctl(shm_id, IPC_RMID, 0);
    }

    int shq_id = msgget(shq_key, 0600);
    if (shq_id > 0) {
        ret2 = msgctl(shq_id, IPC_RMID, 0);
    }

    return (ret1 == 0) + (ret2 == 0);
}

int32_t recycle_stray_hash_node(struct agent_id_hashtable_t* ht) {
    return chain_hash_recycle_stray_entry(ht->ht_);
}
