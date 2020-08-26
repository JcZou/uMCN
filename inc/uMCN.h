/******************************************************************************
 * Copyright 2020 The Firmament Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#ifndef __UMCN_H__
#define __UMCN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <rtthread.h>

#define MCN_MALLOC(size)            rt_malloc(size)
#define MCN_FREE(ptr)               rt_free(ptr)
#define MCN_ENTER_CRITICAL          rt_enter_critical()
#define MCN_EXIT_CRITICAL           rt_exit_critical()
#define MCN_EVENT_HANDLE            rt_sem_t
#define MCN_SEND_EVENT(event)       rt_sem_release(event)
#define MCN_WAIT_EVENT(event, time) rt_sem_take(event, time)
#define MCN_ASSERT(EX)              RT_ASSERT(EX)

#define MCN_MAX_LINK_NUM 30

typedef struct mcn_node McnNode;
typedef struct mcn_node* McnNode_t;
struct mcn_node {
    volatile uint8_t renewal;
    MCN_EVENT_HANDLE event_t;
    void (*cb)(void* parameter);
    McnNode_t next;
};

typedef struct mcn_hub McnHub;
struct mcn_hub {
    const char* obj_name;
    const uint32_t obj_size;
    void* pdata;
    McnNode_t link_head;
    McnNode_t link_tail;
    uint32_t link_num;
    uint8_t published; // publish flag
    int (*echo)(void* parameter);
    // frequency estimate
    uint32_t last_pub_time;
    float freq;
};

typedef struct mcn_list McnList;
typedef struct mcn_list* McnList_t;
struct mcn_list {
    McnHub* hub_t;
    McnList_t next;
};

/******************* Helper Macro *******************/
#define MCN_ID(_name) (&__mcn_##_name)

#define MCN_DECLARE(_name) extern McnHub __mcn_##_name

#define MCN_DEFINE(_name, _size) \
    McnHub __mcn_##_name = {     \
        .obj_name = #_name,      \
        .obj_size = _size,       \
        .pdata = NULL,           \
        .link_head = NULL,       \
        .link_tail = NULL,       \
        .link_num = 0,           \
        .published = 0,          \
        .last_pub_time = 0,      \
        .freq = 0.0f             \
    }

/******************* API *******************/
rt_err_t mcn_advertise(McnHub* hub, int (*echo)(void* parameter));
McnNode_t mcn_subscribe(McnHub* hub, MCN_EVENT_HANDLE event_t, void (*cb)(void* parameter));
rt_err_t mcn_unsubscribe(McnHub* hub, McnNode_t node);
rt_err_t mcn_publish(McnHub* hub, const void* data);
bool mcn_poll(McnNode_t node_t);
bool mcn_poll_sync(McnNode_t node_t, int32_t timeout);
rt_err_t mcn_copy(McnHub* hub, McnNode_t node_t, void* buffer);
rt_err_t mcn_copy_from_hub(McnHub* hub, void* buffer);
void mcn_node_clear(McnNode_t node_t);

McnList mcn_get_list(void);

#ifdef __cplusplus
}
#endif

#endif
