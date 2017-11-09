#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bo Chen (chenbo@pdx.edu)");
MODULE_DESCRIPTION("CRETE intrinics");

#include "../../lib/vm-comm/custom_instr.c"

EXPORT_SYMBOL(crete_make_concolic);
EXPORT_SYMBOL(crete_kernel_oops);

static int __init crete_intrinsics_tracing_init(void)
{
    printk(KERN_INFO "crete-intrinsics-tracing.ko Loaded!\n");
    return 0;
}

static void __exit crete_intrinsics_tracing_exit(void)
{
}

module_init(crete_intrinsics_tracing_init);
module_exit(crete_intrinsics_tracing_exit);
