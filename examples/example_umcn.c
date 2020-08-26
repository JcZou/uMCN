#include <rtthread.h>
#include <finsh.h>

#include <uMCN.h>

typedef struct {
    int a;
    char str[20];
} test_data;

static rt_thread_t tid0;
static McnNode_t my_nod;
static McnNode_t my_nod_sync;
static rt_sem_t event;

MCN_DEFINE(my_mcn_topic, sizeof(test_data));

static void test_entry(void* parameter)
{
	test_data read_data;
	
	memset(&read_data, 0, sizeof(read_data));
	
	while(1){
		if(mcn_poll(my_nod)){
			mcn_copy(MCN_ID(my_mcn_topic), my_nod, &read_data);
			rt_kprintf("get topic, a=%d str=%s\n", read_data.a, read_data.str);
			break;
		}		
		rt_thread_delay(10);
	}
	
	memset(&read_data, 0, sizeof(read_data));
	
	if(mcn_poll_sync(my_nod_sync, RT_WAITING_FOREVER)){
		mcn_copy(MCN_ID(my_mcn_topic), my_nod_sync, &read_data);
		rt_kprintf("get sync topic, a=%d str=%s\n", read_data.a, read_data.str);
	}
}

int uMCN_test(int argc, char** argv)
{	
	test_data my_data = {.a=777, .str="Hello uMCN"};
	
	mcn_advertise(MCN_ID(my_mcn_topic), RT_NULL);
	
	my_nod = mcn_subscribe(MCN_ID(my_mcn_topic), RT_NULL, RT_NULL);
	
	event = rt_sem_create("my_event", 0, RT_IPC_FLAG_FIFO);
	my_nod_sync = mcn_subscribe(MCN_ID(my_mcn_topic), event, RT_NULL);
	
	tid0 = rt_thread_create("mcn_test",
			test_entry, RT_NULL,
			1024, RT_THREAD_PRIORITY_MAX / 2, 20);
    if (tid0 != RT_NULL)
        rt_thread_startup(tid0);
    
	// publish uMCN topic
	mcn_publish(MCN_ID(my_mcn_topic), &my_data);
	rt_kprintf("publish [my_mcn_topic] topic: a=%d str=%s\n", my_data.a, my_data.str);

    return 0;
}
MSH_CMD_EXPORT(uMCN_test, uMCN API test);
