#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>

static void (*_crete_make_concolic)(void*, size_t, const char *);
static int ret_handler_make_concolic(struct kretprobe_instance *ri, struct pt_regs *regs);

#define __CRETE_DEF_KPROBE(func_name)                                                              \
        static int entry_handler_##func_name(struct kretprobe_instance *ri, struct pt_regs *regs); \
        static struct kretprobe kretp_##func_name = {                                              \
                .kp.symbol_name = #func_name,                                                      \
                .entry_handler = entry_handler_##func_name,                                        \
                .handler = ret_handler_##func_name,                                              \
        };

#define __CRETE_DEF_KPROBE_RET_CONCOLIC(func_name)                                                 \
        static struct kretprobe kretp_##func_name = {                                              \
                .kp.symbol_name = #func_name,                                                      \
                .handler = ret_handler_make_concolic,                                              \
        };

#define __CRETE_REG_KPROBE(func_name)                                          \
        {                                                                      \
            if(register_kretprobe(&kretp_##func_name))                         \
            {                                                                  \
                printk(KERN_INFO "kprobe register failed for "#func_name"\n"); \
                return -1;                                                     \
            }                                                                  \
        }

#define __CRETE_UNREG_KPROBE(func_name)  register_kretprobe(&kretp_##func_name);

/* ------------------------------- */
// Define interested functions to hook
__CRETE_DEF_KPROBE_RET_CONCOLIC(__kmalloc);
//__CRETE_DEF_KPROBE_RET_CONCOLIC(__vmalloc);

static inline int register_probes(void)
{
    __CRETE_REG_KPROBE(__kmalloc);
//    __CRETE_REG_KPROBE(__vmalloc);

    return 0;
}

static inline void unregister_probes(void)
{
    __CRETE_UNREG_KPROBE(__kmalloc);
//    __CRETE_UNREG_KPROBE(__vmalloc);
}

/* ------------------------------- */
// Define entry handlers for each interested function

//static int entry_handler___kmalloc(struct kretprobe_instance *ri, struct pt_regs *regs)
//{
//    char *sp_regs = (char *)kernel_stack_pointer(regs);
//    printk(KERN_INFO "entry_handler_kmalloc() entered.\n");
//    _crete_make_concolic(&regs->cx, sizeof(int), "e1000_ioctl_arg3");
//    return 0;
//}

static int ret_handler_make_concolic(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    if(regs->ax == regs_return_value(regs))
    {
//        printk(KERN_INFO "ret_handler \'%s\': ret = %p",
//                ri->rp->kp.symbol_name, (void *)regs->ax);
        _crete_make_concolic(&regs->ax, sizeof(regs->ax), ri->rp->kp.symbol_name);
    }
//    else {
//        printk(KERN_INFO  "[CRETE ERROR] Wrong assumption on return value convention about \'%s\'\n",
//                ri->rp->kp.symbol_name);
//    }

    return 0;
}
/* ------------------------------- */

static inline int init_crete_intrinsics(void)
{
    _crete_make_concolic = (void *)kallsyms_lookup_name("crete_make_concolic");

    if (!(_crete_make_concolic)) {
        printk(KERN_INFO "[crete] not all function found, please check crete-intrinsics.ko\n");
        return -1;
    }

    return 0;
}

static int __init kprobe_init(void)
{
    if(init_crete_intrinsics())
        return -1;

    if(register_probes())
        return -1;

    return 0;
}

static void __exit kprobe_exit(void)
{
    unregister_probes();
}

module_init(kprobe_init)
module_exit(kprobe_exit)
