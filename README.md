## uMCN消息订阅/发布模块
uMCN (Micro Multi-Communication Node) 提供基于订阅/发布模式的安全跨进程通信方式，为各个线程，模块之间进行信息交换。 uMCN为[Firmament Autopilot](https://github.com/FirmamentPilot/FMT_Firmware)项目的一个组件，更详细的文档内容，请查看[项目文档](https://firmamentpilot.github.io/docs/module/)

### 定义消息
定义一条新的uMCN只需简单几步。这里举例说明定义一条新的消息，消息名称为`my_mcn_topic`，消息内容(topic)为：
```c
typedef struct {
	uint32_t a;
	float b;
	int8_t c[4];
} test_data;
```

- 定义消息：在源文件头部（通常为发布该消息的源文件）添加如下定义：
```c
MCN_DEFINE(my_mcn_topic, sizeof(test_data));
```
这里`my_mcn_topic`为该消息的名字，`sizeof(test_data)`为消息内容的长度。uMCN对消息的长度和类型没有限制，所以理论上可以用uMCN传递任意消息类型。同时uMCN支持一个消息同时有多个发布者 (publisher) 和订阅者 (subscriber)。但是注意，同一个消息(名称)不可被重复定义，不然编译时会报重复定义的错误。

- 注册消息：调用`mcn_advertise`函数注册该条消息：
```c
mcn_advertise(MCN_ID(my_mcn_topic), _my_mcn_topic_echo);
```
这里`my_mcn_topic`同样为该消息的名字，`MCN_ID`是一个宏，用来根据消息名称得到`McnHub`节点。`_my_mcn_topic_echo`为该条消息的打印函数。当在控制台输入指令`mcn echo my_mcn_topic`，将调用该条消息的打印函数来打印消息内容。用户可以自定义打印函数来输出想要数据格式。比如`my_mcn_topic`的打印函数如下这样定义：
```c
static int _my_mcn_topic_echo(void* param)
{
	test_data data;
	if(mcn_copy_from_hub((McnHub*)param, &data) == RT_EOK){
		console_printf("a:%d b:%f c:%c %c %c %c\n", data.a, data.b,
						data.c[0], data.c[1], data.c[2], data.c[3]);
	}
	return 0;
}
```

### 订阅消息
订阅者要获取消息的数据，首先需要先订阅消息，通过`mcn_subscribe`函数实现消息的订阅。uMCN支持同步/异步消息订阅，同步方式需要在订阅消息的时候传入一个消息句柄`event_t`用于线程同步。订阅消息遵循以下几个步骤：

- 声明消息：如果是在非定义该条消息的源文件中，则需要先申明该条消息。比如对于我们刚刚建立的`my_mcn_topic`，需要在订阅消息的源文件头部添加
```c
MCN_DECLARE(my_mcn_topic);
```

- 订阅消息：调用`mcn_subscribe(McnHub* hub, MCN_EVENT_HANDLE event_t, void (*cb)(void* parameter))`函数来订阅消息。
其中`cb`为消息发布的回调函数，在每次发布消息时，回调函数将被调用 (注意: 回调函数将在发布消息的线程中被调用)。`event_t`为用于消息同步的事件句柄，这里一般用系统的信号量(semaphore)实现。当订阅成功后，函数将返回消息节点句柄`McnNode_t`。

这里分别对同步/异步的消息订阅举例：

**同步订阅**

```c
rt_sem_t event = rt_sem_create("my_event", 0, RT_IPC_FLAG_FIFO);
McnNode_t my_nod = mcn_subscribe(MCN_ID(my_mcn_topic), event, NULL);
```
**异步订阅**

```c
McnNode_t my_nod = mcn_subscribe(MCN_ID(my_mcn_topic), NULL, NULL);
```

### 发布消息
发布消息数据使用`mcn_publish`函数。比如
```c
mcn_publish(MCN_ID(my_mcn_topic), &my_data);
```
这里`my_data`为要发布的数据，其类型为`test_data`。注意这里发布消息的类型需跟``MCN_DEFINE()``定义的消息数据类型一致，否则将发生非预期的后果。

### 读取消息
对于同步/异步的消息订阅方式，读取消息时也分为同步和异步方式。

对于同步订阅方式，调用`mcn_poll_sync`函数查询是否收到新的数据。如果没有新的数据，则将当前线程挂起，直到收到数据或者等待超时返回。当有新的数据到来后，调用`mcn_copy`函数读取数据。例如：
```c
test_data read_data;
if(mcn_poll_sync(my_nod, RT_WAIT_FOREVER)){
	mcn_copy(MCN_ID(my_mcn_topic), my_nod, &read_data);
}
```
这里`my_nod`为消息订阅时返回的消息节点句柄。

如果是采用异步订阅方式，使用`mcn_poll`函数查询是否有新的数据，该函数会立马返回。
```c
test_data read_data;
if(mcn_poll(my_nod){
	mcn_copy(MCN_ID(my_mcn_topic), my_nod, &read_data);
}
```
