/* Helper functions added by crete */

#include "cpu.h"
#include "exec/helper-proto.h"
#include "runtime-dump/custom-instructions.h"
#include "runtime-dump/runtime-dump.h"
#include "runtime-dump/tci_analyzer.h"

enum CreteDebugFlag
{
    CRETE_DEBUG_FLAG_ENABLE = 0,
    CRETE_DEBUG_FLAG_DISABLE
};

void helper_crete_make_symbolic(void)
{
    // Do nothing. Just a keyword for Klee to catch.
#if defined(CRETE_DEP_ANALYSIS) || 1
    crete_tci_mark_block_symbolic();
//    printf("helper_crete_make_symbolic called in op_helper.c");
    assert(flag_rt_dump_enable &&
            "Must be capture-enable at this point, so long as dependency "
            "analysis remains a subset of other code selection");
#endif // defined(CRETE_DEP_ANALYSIS)
}

void helper_crete_debug_print_f32(target_ulong eax)
{
//    (void)eax;
    float* tmp = (float*)&eax;
    printf("helper_crete_debug_print_f32 called with %f!\n", *tmp);
    // Do nothing. Just a keyword for Klee to catch.
}

void helper_crete_debug_print_buf(target_ulong eax, target_ulong ecx, target_ulong edx)
{
//    printf("helper_crete_debug_print_buf called with %x, %x, %x!\n", eax, ecx, edx);
}

void helper_crete_debug_assert_is_concolic(target_ulong eax, target_ulong ecx, target_ulong edx)
{
//    printf("helper_crete_debug_assert_is_concolic called with %x, %x, %x!\n", eax, ecx, edx);
}

void helper_crete_debug_monitor_concolic_status(target_ulong eax, target_ulong ecx, target_ulong edx)
{
//    printf("helper_crete_debug_monitor_concolic_status called with %x, %x, %x!\n", eax, ecx, edx);
}

void helper_crete_make_concrete(target_ulong eax, target_ulong ecx, target_ulong edx)
{
//    printf("helper_crete_make_concrete called with %x, %x, %x!\n", eax, ecx, edx);
}

void helper_crete_debug_monitor_value(target_ulong eax, target_ulong ecx, target_ulong edx)
{
//    printf("helper_crete_debug_monitor_value called with %x, %x, %x!\n", eax, ecx, edx);
}

void helper_crete_debug_monitor_set_flag(target_ulong eax, target_ulong ecx)
{
//    printf("helper_crete_debug_monitor_set_flag called with %x, %x!\n", eax, ecx);
}

void helper_crete_debug_capture(target_ulong eax)
{
//    printf("helper_crete_debug_capture called: %d!\n", eax);
    enum CreteDebugFlag flag = (enum CreteDebugFlag)eax;
    switch(flag)
    {
    case CRETE_DEBUG_FLAG_ENABLE:
        crete_flag_capture_enabled = 1;
        crete_set_capture_enabled(g_crete_flags, 1);
        break;
    case CRETE_DEBUG_FLAG_DISABLE:
        crete_flag_capture_enabled = 0;
        crete_set_capture_enabled(g_crete_flags, 0);
        break;
    default:
        assert(0 && "CRETE debug flag not recognized!");
    }
}

void helper_crete_assume_begin(void)
{
    printf("crete_assume_begin called!\n");
    crete_tci_mark_block_symbolic();
    // Do nothing. Just a keyword for Klee to catch.
}
void helper_crete_assume(target_ulong eax)
{
    printf("crete_assume called!\n");
    // Do nothing. Just a keyword for Klee to catch.
    crete_tci_mark_block_symbolic();
}

void helper_crete_custom_instruction_handler(uint64_t arg)
{
    crete_custom_instruction_handler(arg);
}
