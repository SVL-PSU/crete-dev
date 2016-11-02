#ifndef CUSTOM_INSTRUCTIONS_H
#define CUSTOM_INSTRUCTIONS_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif
/*****************************/
/* Functions for QEMU c code */
void crete_custom_instruction_handler(uint64_t arg);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>

enum CreteFileType {
    CRETE_FILE_TYPE_LLVM_LIB,
    CRETE_FILE_TYPE_LLVM_TEMPLATE,
};

int crete_is_pc_in_exclude_filter_range(uint64_t pc);
int crete_is_pc_in_include_filter_range(uint64_t pc);
#endif

#endif
