#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bo Chen (chenbo@pdx.edu)");
MODULE_DESCRIPTION("CRETE probes for kernel API functions to inject concolic values");

//#define CRETE_ENABLE_DEBUG

#ifdef CRETE_ENABLE_DEBUG
#define CRETE_DBG(x) do { x } while(0)
#else
#define CRETE_DBG(x) do { } while(0)
#endif

struct TargetModuleInfo
{
    size_t m_name_size;
    char *m_name;
    int   m_mod_loaded;
    struct module m_mod;
};

static struct TargetModuleInfo target_module = {
        .m_name_size = 0,
        .m_name = "",
        .m_mod_loaded = 0,
};

module_param_named(target_module, target_module.m_name, charp, 0);
MODULE_PARM_DESC(target_module, "The name of target module to enable probe on kernel APIs");

static void (*_crete_make_concolic)(void*, size_t, const char *);
static void (*_crete_kernel_oops)(void);

static int ret_handler_make_concolic(struct kretprobe_instance *ri, struct pt_regs *regs);

#define __CRETE_DEF_KPROBE(func_name)                                                              \
        static int entry_handler_##func_name(struct kretprobe_instance *ri, struct pt_regs *regs); \
        static int ret_handler_##func_name(struct kretprobe_instance *ri, struct pt_regs *regs);   \
        static struct kretprobe kretp_##func_name = {                                              \
                .kp.symbol_name = #func_name,                                                      \
                .entry_handler = entry_handler_##func_name,                                        \
                .handler = ret_handler_##func_name,                                                \
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
__CRETE_DEF_KPROBE(oops_enter);

__CRETE_DEF_KPROBE_RET_CONCOLIC(__kmalloc) // kmalloc, kcalloc
__CRETE_DEF_KPROBE_RET_CONCOLIC(__vmalloc)
__CRETE_DEF_KPROBE_RET_CONCOLIC(vzalloc)
__CRETE_DEF_KPROBE_RET_CONCOLIC(__alloc_skb) // alloc_skb
__CRETE_DEF_KPROBE_RET_CONCOLIC(__napi_alloc_skb)
__CRETE_DEF_KPROBE_RET_CONCOLIC(__alloc_pages_nodemask)
__CRETE_DEF_KPROBE_RET_CONCOLIC(netdev_alloc_frag)
__CRETE_DEF_KPROBE_RET_CONCOLIC(alloc_etherdev_mqs)
//__CRETE_DEF_KPROBE_RET_CONCOLIC()

static inline int register_probes(void)
{
    __CRETE_REG_KPROBE(oops_enter);

    __CRETE_REG_KPROBE(__kmalloc);
    __CRETE_REG_KPROBE(__vmalloc);
    __CRETE_REG_KPROBE(vzalloc);
    __CRETE_REG_KPROBE(__alloc_skb);
    __CRETE_REG_KPROBE( __napi_alloc_skb);
    __CRETE_REG_KPROBE(__alloc_pages_nodemask);
    __CRETE_REG_KPROBE(netdev_alloc_frag);
    __CRETE_REG_KPROBE(alloc_etherdev_mqs);
//    __CRETE_REG_KPROBE();

    return 0;
}

static inline void unregister_probes(void)
{
    __CRETE_UNREG_KPROBE(oops_enter);

    __CRETE_UNREG_KPROBE(__kmalloc);
    __CRETE_UNREG_KPROBE(__vmalloc);
    __CRETE_UNREG_KPROBE(vzalloc);
    __CRETE_UNREG_KPROBE(__alloc_skb);
    __CRETE_UNREG_KPROBE( __napi_alloc_skb);
    __CRETE_UNREG_KPROBE(__alloc_pages_nodemask);
    __CRETE_UNREG_KPROBE(netdev_alloc_frag);
    __CRETE_UNREG_KPROBE(alloc_etherdev_mqs);
//    __CRETE_UNREG_KPROBE();
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

// Only inject concolic values if:
// 1. its caller is within target_module AND
// 2. the convention on return value holds
static int ret_handler_make_concolic(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    if(!(target_module.m_mod_loaded &&
         within_module((unsigned long)ri->ret_addr, &target_module.m_mod)))
    {
        return 0;
    }

    if(regs->ax != regs_return_value(regs))
    {
        printk(KERN_INFO  "[CRETE ERROR] Wrong assumption on return value convention about \'%s\'\n",
                ri->rp->kp.symbol_name);
        return 0;
    }

    if(!_crete_make_concolic)
    {
        CRETE_DBG(printk(KERN_INFO  "[CRETE ERROR] \'_crete_make_concolic() is not initialized\'\n"););
        return 0;
    }

    CRETE_DBG(printk(KERN_INFO "ret_handler \'%s\': ret = %p", ri->rp->kp.symbol_name, (void *)regs->ax););

    _crete_make_concolic(&regs->ax, sizeof(regs->ax), ri->rp->kp.symbol_name);

    return 0;
}

static int entry_handler_oops_enter(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    _crete_kernel_oops();

    return 0;
}

static int ret_handler_oops_enter(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    return 0;
}

/* ------------------------------- */

static int crete_kapi_module_event(struct notifier_block *self, unsigned long event, void *data)
{
    struct module *m = data;

    if(strlen(m->name) != target_module.m_name_size)
        return NOTIFY_DONE;

    if(strncmp(m->name, target_module.m_name, target_module.m_name_size) != 0)
        return NOTIFY_DONE;

    switch (event) {
    case MODULE_STATE_COMING:
        printk(KERN_INFO "MODULE_STATE_COMING: %s\n", m->name);
        target_module.m_mod_loaded = 1;
        target_module.m_mod.core_layout = m->core_layout;
        target_module.m_mod.init_layout = m->init_layout;
        break;

    case MODULE_STATE_GOING:
        printk(KERN_INFO "MODULE_STATE_GOING: %s\n", m->name);
        target_module.m_mod_loaded = 0;
        break;

    case MODULE_STATE_LIVE:
    default:
        break;
    }

    return NOTIFY_DONE;
}

static struct notifier_block crete_kapi_module_probe= {
        .notifier_call = crete_kapi_module_event,
        .priority = 1,
};

static inline int init_crete_intrinsics(void)
{
    _crete_make_concolic = (void *)kallsyms_lookup_name("crete_make_concolic");
    _crete_kernel_oops = (void *)kallsyms_lookup_name("crete_kernel_oops");

    if (!(_crete_make_concolic && _crete_kernel_oops)) {
        printk(KERN_INFO "[crete] not all function found, please check crete-intrinsics.ko\n");
        return -1;
    }

    return 0;
}

static int __init crete_kprobe_init(void)
{
    if(init_crete_intrinsics())
        return -1;

    if(register_probes())
        return -1;

    target_module.m_name_size = strlen(target_module.m_name);
    register_module_notifier(&crete_kapi_module_probe);

    CRETE_DBG(printk(KERN_INFO "size = %zu, name = %s\n", target_module.m_name_size, target_module.m_name););

    return 0;
}

static void __exit crete_kprobe_exit(void)
{
    unregister_module_notifier(&crete_kapi_module_probe);
    unregister_probes();
}

module_init(crete_kprobe_init)
module_exit(crete_kprobe_exit)
