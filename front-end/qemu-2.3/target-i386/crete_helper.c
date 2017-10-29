/* Helper functions added by crete */

#include "cpu.h"
#include "exec/helper-proto.h"
#include "runtime-dump/custom-instructions.h"
#include "runtime-dump/runtime-dump.h"
#include "runtime-dump/tci_analyzer.h"

void helper_crete_custom_instruction_handler(uint64_t arg)
{
    crete_custom_instruction_handler(arg);
}

void helper_crete_make_concolic_internal(target_ulong guest_addr, target_ulong size, target_ulong name_guest_addr)
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
