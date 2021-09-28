
#include <rtthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <optparse.h>
#include <shell.h>

#include "uMCN.h"

#define STRING_COMPARE(str1, str2)      (strcmp(str1, str2) == 0)
#define PRINT_USAGE(cmd, usage)         rt_kprintf("usage: %s %s\n", #cmd, #usage)
#define PRINT_STRING(str)               rt_kprintf("%s", str)
#define PRINT_ACTION(action, len, desc) rt_kprintf(" %-" #len "s  %s\n", action, desc)
#define COMMAND_USAGE(cmd, usage)       rt_kprintf("usage: %s %s\n", cmd, usage)
#define SHELL_COMMAND(cmd, desc)        rt_kprintf(" %-10s  %s\n", cmd, desc)
#define SHELL_OPTION(opt, desc)         rt_kprintf(" %-15s  %s\n", opt, desc)

enum {
    SYSCMD_ALIGN_LEFT,
    SYSCMD_ALIGN_MIDDLE,
    SYSCMD_ALIGN_RIGHT
};

extern struct finsh_shell* shell;

static rt_device_t console_dev;

static void show_usage(void)
{
    COMMAND_USAGE("mcn", "<command> [options]");

    PRINT_STRING("\ncommand:\n");
    SHELL_COMMAND("list", "List all uMCN topics.");
    SHELL_COMMAND("echo", "Echo a uMCN topic.");
    SHELL_COMMAND("suspend", "Suspend a uMCN topic.");
    SHELL_COMMAND("resume", "Resume a uMCN topic.");
}

static void show_echo_usage(void)
{
    COMMAND_USAGE("mcn echo", "<topic> [options]");

    PRINT_STRING("\noptions:\n");
    SHELL_OPTION("-n, --number", "Set topic echo number, e.g, -n 10 will echo 10 times.");
    SHELL_OPTION("-p, --period", "Set topic echo period (ms), -p 0 inherits topic period");
}

static void show_suspend_usage(void)
{
    COMMAND_USAGE("mcn suspend", "<topic>");
}

static void show_resume_usage(void)
{
    COMMAND_USAGE("mcn resume", "<topic>");
}

rt_inline void object_split(int len)
{
    while (len--)
        rt_kprintf("-");
}

void list_print_char(const char c, int cnt)
{
    while (cnt--)
        rt_device_write(console_dev, 0, &c, 1);
}

void list_printf(const char pad, uint32_t len, uint8_t align, const char* fmt, ...)
{
    va_list args;
    char buffer[100];
    int length;

    va_start(args, fmt);
    length = vsprintf(buffer, fmt, args);
    va_end(args);

    if (len <= length) {
        rt_device_write(console_dev, 0, buffer, length);
        return;
    }

    if (align == SYSCMD_ALIGN_LEFT) {
        rt_device_write(console_dev, 0, buffer, length);
        list_print_char(pad, len - length);
    } else if (align == SYSCMD_ALIGN_MIDDLE) {
        uint32_t hl = (len - length + 1) / 2;
        list_print_char(pad, hl);
        rt_device_write(console_dev, 0, buffer, length);
        list_print_char(pad, (len - length) - hl);
    } else {
        list_print_char(pad, len - length);
        rt_device_write(console_dev, 0, buffer, length);
    }
}

static int name_maxlen(const char* title)
{
    int max_len = strlen(title);

    McnList_t ite = mcn_get_list();
    while (1) {
        McnHub_t hub = mcn_iterate(&ite);
        if (hub == NULL) {
            break;
        }
        int len = strlen(hub->obj_name);

        if (len > max_len) {
            max_len = len;
        }
    }

    return max_len;
}

static void list_topic(void)
{
    uint32_t max_len = name_maxlen("Topic") + 2;

    rt_kprintf("%-*.s    #SUB   Freq(Hz)   Echo   Suspend\n", max_len - 2, "Topic");
    object_split(max_len);
    rt_kprintf(" ------ ---------- ------ ---------\n");

    McnList_t ite = mcn_get_list();
    for (McnHub_t hub = mcn_iterate(&ite); hub != NULL; hub = mcn_iterate(&ite)) {
        list_printf(' ', max_len, SYSCMD_ALIGN_LEFT, hub->obj_name); rt_kprintf(" ");
        list_printf(' ', strlen("#SUB") + 2, SYSCMD_ALIGN_MIDDLE, "%d", (int)hub->link_num); rt_kprintf(" ");
        list_printf(' ', strlen("Freq(Hz)") + 2, SYSCMD_ALIGN_MIDDLE, "%.1f", hub->freq); rt_kprintf(" ");
        list_printf(' ', strlen("Echo") + 2, SYSCMD_ALIGN_MIDDLE, "%s", hub->echo ? "true" : "false"); rt_kprintf(" ");
        list_printf(' ', strlen("Suspend") + 2, SYSCMD_ALIGN_MIDDLE, "%s", hub->suspend ? "true" : "false"); rt_kprintf("\n");
    }
}

static int suspend_topic(struct optparse options)
{
    char* arg;
    int option;
    struct optparse_long longopts[] = {
        { "help", 'h', OPTPARSE_NONE },
        { NULL } /* Don't remove this line */
    };

    while ((option = optparse_long(&options, longopts, NULL)) != -1) {
        switch (option) {
        case 'h':
            show_suspend_usage();
            return EXIT_SUCCESS;
        case '?':
            rt_kprintf("%s: %s\n", "mcn echo", options.errmsg);
            return EXIT_FAILURE;
        }
    }

    if ((arg = optparse_arg(&options)) == NULL) {
        show_suspend_usage();
        return EXIT_FAILURE;
    }

    McnList_t ite = mcn_get_list();
    McnHub_t target_hub = NULL;

    while (1) {
        McnHub_t hub = mcn_iterate(&ite);
        if (hub == NULL) {
            break;
        }
        if (strcmp(hub->obj_name, arg) == 0) {
            target_hub = hub;
            break;
        }
    }

    if (target_hub == NULL) {
        rt_kprintf("can not find topic %s\n", arg);
        return EXIT_FAILURE;
    }

    mcn_suspend(target_hub);

    return EXIT_SUCCESS;
}

static int resume_topic(struct optparse options)
{
    char* arg;
    int option;
    struct optparse_long longopts[] = {
        { "help", 'h', OPTPARSE_NONE },
        { NULL } /* Don't remove this line */
    };

    while ((option = optparse_long(&options, longopts, NULL)) != -1) {
        switch (option) {
        case 'h':
            show_resume_usage();
            return EXIT_SUCCESS;
        case '?':
            rt_kprintf("%s: %s\n", "mcn echo", options.errmsg);
            return EXIT_FAILURE;
        }
    }

    if ((arg = optparse_arg(&options)) == NULL) {
        show_resume_usage();
        return EXIT_FAILURE;
    }

    McnList_t ite = mcn_get_list();
    McnHub_t target_hub = NULL;

    while (1) {
        McnHub_t hub = mcn_iterate(&ite);
        if (hub == NULL) {
            break;
        }
        if (strcmp(hub->obj_name, arg) == 0) {
            target_hub = hub;
            break;
        }
    }

    if (target_hub == NULL) {
        rt_kprintf("can not find topic %s\n", arg);
        return EXIT_FAILURE;
    }

    mcn_resume(target_hub);

    return EXIT_SUCCESS;
}

static int echo_topic(struct optparse options)
{
    char* arg;
    int option;
    struct optparse_long longopts[] = {
        { "help", 'h', OPTPARSE_NONE },
        { "number", 'n', OPTPARSE_REQUIRED },
        { "period", 'p', OPTPARSE_REQUIRED },
        { NULL } /* Don't remove this line */
    };

#if defined(RT_USING_DEVICE) && !defined(RT_USING_POSIX)
    uint32_t cnt = 0xFFFFFFFF;
#else
    uint32_t cnt = 1;
#endif
    uint32_t period = 500;

    while ((option = optparse_long(&options, longopts, NULL)) != -1) {
        switch (option) {
        case 'h':
            show_echo_usage();
            return EXIT_SUCCESS;
        case 'n':
            cnt = atoi(options.optarg);
            break;
        case 'p':
            period = atoi(options.optarg);
            break;
        case '?':
            rt_kprintf("%s: %s\n", "mcn echo", options.errmsg);
            return EXIT_FAILURE;
        }
    }

    if ((arg = optparse_arg(&options)) == NULL) {
        show_echo_usage();
        return EXIT_FAILURE;
    }

    McnList_t ite = mcn_get_list();
    McnHub_t target_hub = NULL;
    while (1) {
        McnHub_t hub = mcn_iterate(&ite);
        if (hub == NULL) {
            break;
        }
        if (strcmp(hub->obj_name, arg) == 0) {
            target_hub = hub;
            break;
        }
    }

    if (target_hub == NULL) {
        rt_kprintf("can not find topic %s\n", arg);
        return EXIT_FAILURE;
    }

    if (target_hub->echo == NULL) {
        rt_kprintf("there is no topic echo function defined!\n");
        return EXIT_FAILURE;
    }

    McnNode_t node = mcn_subscribe(target_hub, NULL, NULL);

    if (node == NULL) {
        rt_kprintf("mcn subscribe fail\n");
        return EXIT_FAILURE;
    }

    while (cnt) {
#if defined(RT_USING_DEVICE) && !defined(RT_USING_POSIX)
        /* type any key to exit */
        if (rt_sem_trytake(&shell->rx_sem) == RT_EOK) {
            int ch;
            while (rt_device_read(shell->device, -1, &ch, 1) == 1)
                ;
            break;
        }
#endif

        if (mcn_poll(node)) {
            /* call custom echo function */
            target_hub->echo(target_hub);
            mcn_node_clear(node);
            cnt--;
        }

        if (period) {
            usleep(period * 1000);
        }
    }

    if (mcn_unsubscribe(target_hub, node) != RT_EOK) {
        rt_kprintf("mcn unsubscribe fail\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int cmd_mcn(int argc, char** argv)
{
    char* arg;
    struct optparse options;
    int res = EXIT_SUCCESS;

    console_dev = rt_console_get_device();

    optparse_init(&options, argv);

    arg = optparse_arg(&options);
    if (arg) {
        if (STRING_COMPARE(arg, "list")) {
            list_topic();
        } else if (STRING_COMPARE(arg, "echo")) {
            res = echo_topic(options);
        } else if (STRING_COMPARE(arg, "suspend")) {
            res = suspend_topic(options);
        } else if (STRING_COMPARE(arg, "resume")) {
            res = resume_topic(options);
        } else {
            show_usage();
        }
    } else {
        show_usage();
    }

    return res;
}
MSH_CMD_EXPORT_ALIAS(cmd_mcn, __cmd_mcn, uMCN topics operations);