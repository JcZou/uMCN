/******************************************************************************
 * Copyright 2021 The Firmament Authors. All Rights Reserved.
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

#ifndef UMCN_H__
#define UMCN_H__

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

#define MCN_MAX_LINK_NUM        30
#define MCN_FREQ_EST_WINDOW_LEN 5

#ifndef bool
typedef int bool;
#endif

#ifndef true
#define true (1U)
#endif

#ifndef false
#define false (0U)
#endif

typedef struct mcn_node McnNode;
typedef struct mcn_node* McnNode_t;
struct mcn_node {
    volatile uint8_t renewal;
    MCN_EVENT_HANDLE event;
    void (*pub_cb)(void* parameter);
    McnNode_t next;
};

typedef struct mcn_hub McnHub;
typedef struct mcn_hub* McnHub_t;
struct mcn_hub {
    const char* obj_name;
    const uint32_t obj_size;
    void* pdata;
    McnNode_t link_head;
    McnNode_t link_tail;
    uint32_t link_num;
    uint8_t published;
    uint8_t suspend;
    int (*echo)(void* parameter);
    /* publish freq estimate */
    float freq;
    uint16_t freq_est_window[MCN_FREQ_EST_WINDOW_LEN];
    uint16_t window_index;
};

typedef struct mcn_list McnList;
typedef struct mcn_list* McnList_t;
struct mcn_list {
    McnHub_t hub;
    McnList_t next;
};

/* Obtain uMCN hub according to name */
#define MCN_HUB(_name) (&__mcn_##_name)
/* Declare a uMCN topic. Declare the topic at places where you need use it */
#define MCN_DECLARE(_name) extern McnHub __mcn_##_name
/* Define a uMCN topic. A topic should only be defined once */
#define MCN_DEFINE(_name, _size) \
    McnHub __mcn_##_name = {     \
        .obj_name = #_name,      \
        .obj_size = _size,       \
        .pdata = NULL,           \
        .link_head = NULL,       \
        .link_tail = NULL,       \
        .link_num = 0,           \
        .published = 0,          \
        .suspend = 0,            \
        .freq = 0.0f             \
    }

int mcn_init(void);
rt_err_t mcn_advertise(McnHub_t hub, int (*echo)(void* parameter));
McnNode_t mcn_subscribe(McnHub_t hub, MCN_EVENT_HANDLE event, void (*pub_cb)(void* parameter));
rt_err_t mcn_unsubscribe(McnHub_t hub, McnNode_t node);
rt_err_t mcn_publish(McnHub_t hub, const void* data);
bool mcn_poll(McnNode_t node_t);
bool mcn_poll_sync(McnNode_t node_t, int32_t timeout);
rt_err_t mcn_copy(McnHub_t hub, McnNode_t node_t, void* buffer);
rt_err_t mcn_copy_from_hub(McnHub_t hub, void* buffer);
void mcn_suspend(McnHub_t hub);
void mcn_resume(McnHub_t hub);
McnList_t mcn_get_list(void);
McnHub_t mcn_iterate(McnList_t* ite);
void mcn_node_clear(McnNode_t node_t);

#ifdef __cplusplus
}
#endif

#endif
