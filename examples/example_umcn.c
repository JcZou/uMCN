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

#include <finsh.h>
#include <rtthread.h>
#include <uMCN.h>

typedef struct {
    char str[20];
    unsigned long count;
} count_topic_t;

typedef struct {
    rt_tick_t tick;
} systick_topic_t;

static rt_thread_t tid0;
static McnNode_t count_nod;
static McnNode_t systick_nod;
static rt_sem_t event;

MCN_DEFINE(count, sizeof(count_topic_t));
MCN_DEFINE(systick, sizeof(systick_topic_t));

static int count_topic_echo(void* parameter)
{
    count_topic_t count_topic;

    if (mcn_copy_from_hub((McnHub*)parameter, &count_topic) != RT_EOK) {
        return -1;
    }

    rt_kprintf("string:%s count:%lu\n", count_topic.str, count_topic.count);
    return 0;
}

static int systick_topic_echo(void* parameter)
{
    systick_topic_t systick_topic;

    if (mcn_copy_from_hub((McnHub*)parameter, &systick_topic) != RT_EOK) {
        return -1;
    }

    rt_kprintf("tick:%u\n", systick_topic.tick);
    return 0;
}

void count_topic_pub_cb(void* parameter)
{
    count_topic_t count_topic = *(count_topic_t*)parameter;

    rt_kprintf("publish callback, string:%s count:%lu\n", count_topic.str, count_topic.count);

    /* publish callback once */
    mcn_unsubscribe(MCN_HUB(count), count_nod);
}

static void test_entry(void* parameter)
{
    rt_tick_t cnt = 0;
    rt_tick_t sec_tick = rt_tick_from_millisecond(1000);
    count_topic_t count_topic = { .count = 0, .str = "Hello RT-Thread!" };

    while (1) {
        systick_topic_t systick_topic;
        systick_topic.tick = rt_tick_get();

        if (++cnt >= sec_tick) {
            cnt = 0;
            count_topic.count++;
            /* publish count topic in 1Hz */
            mcn_publish(MCN_HUB(count), &count_topic);
        }

        /* publish systick topic at each tick */
        mcn_publish(MCN_HUB(systick), &systick_topic);
        rt_thread_delay(1);
    }
}

int mcn_test(int argc, char** argv)
{
    /* advertise topic and provide echo function */
    mcn_advertise(MCN_HUB(count), count_topic_echo);
    mcn_advertise(MCN_HUB(systick), systick_topic_echo);

    /* subscribe topic in asynchronous mode. 
     * The call back function is called if topic is published */
    count_nod = mcn_subscribe(MCN_HUB(count), RT_NULL, count_topic_pub_cb);
    /* subscribe topic in synchronous mode */
    event = rt_sem_create("my_event", 0, RT_IPC_FLAG_FIFO);
    systick_nod = mcn_subscribe(MCN_HUB(systick), event, RT_NULL);

    tid0 = rt_thread_create("mcn_test",
        test_entry, RT_NULL,
        1024, RT_THREAD_PRIORITY_MAX / 2, 20);
    if (tid0 != RT_NULL)
        rt_thread_startup(tid0);

    /* synchronous wait until topic received */
    if (mcn_poll_sync(systick_nod, RT_WAITING_FOREVER)) {
        systick_topic_t data;
        /* copy topic data */
        mcn_copy(MCN_HUB(systick), systick_nod, &data);
        rt_kprintf("get sync topic, tick=%ld\n", data.tick);
    }

    return 0;
}
MSH_CMD_EXPORT(mcn_test, uMCN API test);
