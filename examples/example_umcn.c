#include <rtthread.h>
#include <finsh.h>

#include <uMCN.h>

typedef struct {
    uint32_t a;
    int8_t b;
} test_data;

MCN_DEFINE(my_mcn_topic, sizeof(test_data));

static int _my_mcn_topic_echo(void* param)
{
    test_data data;
    if(mcn_copy_from_hub((McnHub*)param, &data) == RT_EOK){
        rt_kprintf("a:%d b:%d\n", data.a, data.b);
    }
    return 0;
}

static void publisher(void)
{
	test_data my_data = {2, -1};
	
	mcn_advertise(MCN_ID(my_mcn_topic), _my_mcn_topic_echo);
	
	mcn_publish(MCN_ID(my_mcn_topic), &my_data);
}

static void subscriber(void)
{
	test_data read_data;
	
	McnNode_t my_nod = mcn_subscribe(MCN_ID(my_mcn_topic), NULL, NULL);
	
	if(mcn_poll(my_nod){
		mcn_copy(MCN_ID(my_mcn_topic), my_nod, &read_data);
		rt_kprintf("read data: a:%d b:%d\n", read_data.a, read_data.b);
	}
}

int uMCN_test(int argc, char** argv)
{
    publisher();
	
	subscriber();

    return 0;
}
MSH_CMD_EXPORT(uMCN_test, uMCN API test);
