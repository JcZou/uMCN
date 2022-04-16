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

#include <string.h>
#include <rtthread.h>
#include <uMCN.h>

#define DBG_TAG    "uMCN"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

static McnList __mcn_list = { .hub = RT_NULL, .next = RT_NULL };
static struct rt_timer timer_mcn_freq_est;

/**
 * @brief Topic publish frequency estimator entry
 *
 * @param parameter Unused
 */
static void mcn_freq_est_entry(void* parameter)
{
    for (McnList_t cp = &__mcn_list; cp != RT_NULL; cp = cp->next) {
        McnHub_t hub = cp->hub;
        if (hub == RT_NULL) {
            break;
        }
        /* calculate publish frequency */
        rt_uint32_t cnt = 0;
        for (int i = 0; i < MCN_FREQ_EST_WINDOW_LEN; i++) {
            cnt += hub->freq_est_window[i];
            hub->freq = (float)cnt / MCN_FREQ_EST_WINDOW_LEN;
        }
        /* move window */
        hub->window_index = (hub->window_index + 1) % MCN_FREQ_EST_WINDOW_LEN;
        hub->freq_est_window[hub->window_index] = 0;
    }
}

/**
 * @brief Clear uMCN node renewal flag
 *
 * @param node_t uMCN node
 */
void mcn_node_clear(McnNode_t node_t)
{
    MCN_ASSERT(node_t != RT_NULL);

    if (node_t == RT_NULL) {
        return;
    }

    MCN_ENTER_CRITICAL;
    node_t->renewal = 0;
    MCN_EXIT_CRITICAL;
}

/**
 * @brief Suspend a uMCN topic
 *
 * @param hub uMCN hub
 */
void mcn_suspend(McnHub_t hub)
{
    hub->suspend = 1;
}

/**
 * @brief Resume a uMCN topic
 *
 * @param hub uMCN hub
 */
void mcn_resume(McnHub_t hub)
{
    hub->suspend = 0;
}

/**
 * @brief Get uMCN list
 *
 * @return McnList_t uMCN list pointer
 */
McnList_t mcn_get_list(void)
{
    return &__mcn_list;
}

/**
 * @brief Iterate all uMCN hubs in list
 *
 * @param ite uMCN list pointer
 * @return McnHub_t uMCN hub
 */
McnHub_t mcn_iterate(McnList_t* ite)
{
    McnHub_t hub;
    McnList_t node = *ite;

    if (node == RT_NULL) {
        return RT_NULL;
    }
    hub = node->hub;
    *ite = node->next;

    return hub;
}

/**
 * @brief Poll for topic status
 * @note This function would return immediately
 *
 * @param node_t uMCN node
 * @return RT_TRUE Topic updated
 * @return RT_FALSE Topic not updated
 */
rt_bool_t mcn_poll(McnNode_t node_t)
{
    rt_bool_t renewal;

    MCN_ASSERT(node_t != RT_NULL);

    MCN_ENTER_CRITICAL;
    renewal = node_t->renewal;
    MCN_EXIT_CRITICAL;

    return renewal;
}

/**
 * @brief Synchronize poll for topic status
 * @note event must has been provided when subscribe the topic
 *
 * @param node_t uMCN node
 * @param timeout Wait timeout
 * @return RT_TRUE RT_TRUE Topic updated
 * @return RT_FALSE RT_FALSE Topic not updated
 */
rt_bool_t mcn_poll_sync(McnNode_t node_t, rt_int32_t timeout)
{
    MCN_ASSERT(node_t != RT_NULL);
    MCN_ASSERT(node_t->event != RT_NULL);

    return MCN_WAIT_EVENT(node_t->event, timeout) == 0 ? RT_TRUE : RT_FALSE;
}

/**
 * @brief Copy uMCN topic data from hub
 * @note This function will clear the renewal flag
 *
 * @param hub uMCN hub
 * @param node_t uMCN node
 * @param buffer buffer to received the data
 * @return rt_err_t RT_EOK indicates success
 */
rt_err_t mcn_copy(McnHub_t hub, McnNode_t node_t, void* buffer)
{
    MCN_ASSERT(hub != RT_NULL);
    MCN_ASSERT(node_t != RT_NULL);
    MCN_ASSERT(buffer != RT_NULL);

    if (hub->pdata == RT_NULL) {
        /* copy from non-advertised hub */
        return -RT_ERROR;
    }

    if (!hub->published) {
        /* copy before published */
        return -RT_ERROR;
    }

    MCN_ENTER_CRITICAL;
    rt_memcpy(buffer, hub->pdata, hub->obj_size);
    node_t->renewal = 0;
    MCN_EXIT_CRITICAL;

    return RT_EOK;
}

/**
 * @brief Copy uMCN topic data from hub
 * @note This function will directly copy topic data from hub no matter it has been
 * updated or not and won't clear the renewal flag
 *
 * @param hub
 * @param buffer
 * @return rt_err_t
 */
rt_err_t mcn_copy_from_hub(McnHub_t hub, void* buffer)
{
    MCN_ASSERT(hub != RT_NULL);
    MCN_ASSERT(buffer != RT_NULL);

    if (hub->pdata == RT_NULL) {
        /* copy from non-advertised hub */
        return -RT_ERROR;
    }

    if (!hub->published) {
        /* copy before published */
        return -RT_ERROR;
    }

    MCN_ENTER_CRITICAL;
    rt_memcpy(buffer, hub->pdata, hub->obj_size);
    MCN_EXIT_CRITICAL;

    return RT_EOK;
}

/**
 * @brief Advertise a uMCN topic
 *
 * @param hub uMCN hub
 * @param echo Echo function to print topic contents
 * @return rt_err_t RT_EOK indicates success
 */
rt_err_t mcn_advertise(McnHub_t hub, int (*echo)(void* parameter))
{
    MCN_ASSERT(hub != RT_NULL);

    if (hub->pdata != RT_NULL) {
        /* already advertised */
        return -RT_ERROR;
    }

    MCN_ENTER_CRITICAL;
    hub->pdata = MCN_MALLOC(hub->obj_size);
    hub->echo = echo;

    if (hub->pdata == RT_NULL) {
        return -RT_ENOMEM;
    }

    memset(hub->pdata, 0, hub->obj_size);

    /* update Mcn List */
    McnList_t cp = &__mcn_list;

    while (cp->next != RT_NULL) {
        /* find last node */
        cp = cp->next;
    }

    if (cp->hub != RT_NULL) {
        cp->next = (McnList_t)MCN_MALLOC(sizeof(McnList));

        if (cp->next == RT_NULL)
            return -RT_ENOMEM;

        cp = cp->next;
    }

    cp->hub = hub;
    cp->next = RT_NULL;

    /* init publish freq estimator window */
    memset(hub->freq_est_window, 0, 2 * MCN_FREQ_EST_WINDOW_LEN);
    hub->window_index = 0;

    MCN_EXIT_CRITICAL;

    return RT_EOK;
}

/**
 * @brief Subscribe a uMCN topic
 *
 * @param hub uMCN hub
 * @param event Event handler to provide synchronize poll
 * @param pub_cb Topic published callback function
 * @return McnNode_t Subscribe node, return RT_NULL if fail
 */
McnNode_t mcn_subscribe(McnHub_t hub, MCN_EVENT_HANDLE event, void (*pub_cb)(void* parameter))
{
    MCN_ASSERT(hub != RT_NULL);

    if (hub->link_num >= MCN_MAX_LINK_NUM) {
        LOG_E("mcn link num is already full!");
        return RT_NULL;
    }

    McnNode_t node = (McnNode_t)MCN_MALLOC(sizeof(McnNode));

    if (node == RT_NULL) {
        LOG_E("mcn create node fail!");
        return RT_NULL;
    }

    node->renewal = 0;
    node->event = event;
    node->pub_cb = pub_cb;
    node->next = RT_NULL;

    MCN_ENTER_CRITICAL;

    /* no node link yet */
    if (hub->link_tail == RT_NULL) {
        hub->link_head = hub->link_tail = node;
    } else {
        hub->link_tail->next = node;
        hub->link_tail = node;
    }

    hub->link_num++;
    MCN_EXIT_CRITICAL;

    if (hub->published) {
        /* update renewal flag as it's already published */
        node->renewal = 1;

        if (node->pub_cb) {
            /* if data published before subscribe, then call callback immediately */
            node->pub_cb(hub->pdata);
        }
    }

    return node;
}

/**
 * @brief Unsubscribe a uMCN topic
 *
 * @param hub uMCN hub
 * @param node Subscribe node
 * @return rt_err_t RT_EOK indicates success
 */
rt_err_t mcn_unsubscribe(McnHub_t hub, McnNode_t node)
{
    MCN_ASSERT(hub != RT_NULL);
    MCN_ASSERT(node != RT_NULL);

    /* traverse each node */
    McnNode_t cur_node = hub->link_head;
    McnNode_t pre_node = RT_NULL;

    while (cur_node != RT_NULL) {
        if (cur_node == node) {
            /* find node */
            break;
        }

        pre_node = cur_node;
        cur_node = cur_node->next;
    }

    if (cur_node == RT_NULL) {
        /* can not find */
        return -RT_EEMPTY;
    }

    /* update list */
    MCN_ENTER_CRITICAL;

    if (hub->link_num == 1) {
        hub->link_head = hub->link_tail = RT_NULL;
    } else {
        if (cur_node == hub->link_head) {
            hub->link_head = cur_node->next;
        } else if (cur_node == hub->link_tail) {
            if (pre_node) {
                pre_node->next = RT_NULL;
            }
            hub->link_tail = pre_node;
        } else {
            pre_node->next = cur_node->next;
        }
    }

    /* free current node */
    MCN_FREE(cur_node);
    // cur_node = RT_NULL;
    hub->link_num--;
    MCN_EXIT_CRITICAL;

    return RT_EOK;
}

/**
 * @brief Publish uMCN topic
 *
 * @param hub uMCN hub, which can be obtained by MCN_HUB() macro
 * @param data Data of topic to publish
 * @return rt_err_t RT_EOK indicates success
 */
rt_err_t mcn_publish(McnHub_t hub, const void* data)
{
    MCN_ASSERT(hub != RT_NULL);
    MCN_ASSERT(data != RT_NULL);

    if (hub->pdata == RT_NULL) {
        /* hub is not advertised yet */
        return -RT_ERROR;
    }

    if (hub->suspend) {
        return -RT_ERROR;
    }

    /* update freq estimator window */
    hub->freq_est_window[hub->window_index]++;

    MCN_ENTER_CRITICAL;
    /* copy data to hub */
    rt_memcpy(hub->pdata, data, hub->obj_size);
    /* traverse each node */
    McnNode_t node = hub->link_head;

    while (node != RT_NULL) {
        /* update each node's renewal flag */
        node->renewal = 1;

        /* send out event to wakeup waiting task */
        if (node->event) {
            /* stimulate as mutex */
            if (node->event->value == 0)
                MCN_SEND_EVENT(node->event);
        }

        node = node->next;
    }

    hub->published = 1;
    MCN_EXIT_CRITICAL;

    /* invoke callback func */
    node = hub->link_head;

    while (node != RT_NULL) {
        if (node->pub_cb != RT_NULL) {
            node->pub_cb(hub->pdata);
        }

        node = node->next;
    }

    return RT_EOK;
}

/**
 * @brief Initialize uMCN module
 * 
 * @return int RT_EOK indicates success
 */
int mcn_init(void)
{
    rt_timer_init(&timer_mcn_freq_est, "mcn_freq_est",
        mcn_freq_est_entry,
        RT_NULL,
        RT_TICK_PER_SECOND,
        RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);

    if (rt_timer_start(&timer_mcn_freq_est) != RT_EOK) {
        LOG_E("timer start error!");
        return -RT_ERROR;
    }

    return RT_EOK;
}
INIT_DEVICE_EXPORT(mcn_init);
