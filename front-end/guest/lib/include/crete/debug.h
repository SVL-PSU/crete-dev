#ifndef CRETE_DEBUG_H
#define CRETE_DEBUG_H

#include <stdint.h>

#include <crete/debug_flags.h> // For user convenience.

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief Creates a concrete memory object for the given address.
 * @param addr - Address of target memory.
 * @param size - Size of target memory.
 * @param name - Name of memory object. No restrictions.
 *
 * If a memory object exists in the [addr,addr+size) range, that memory will be merged.
 *
 * Q: What happens if addr points to symbolic memory? Will it be concretized?
 */
void crete_make_concrete(void* addr, size_t size, const char* name);

/**
 * @brief crete_debug_capture
 * @param flag - Flag to set.
 */
void crete_debug_capture(enum CreteDebugFlag flag);

/**
 * @brief Prints a 32 bit floating point value at calling point during symbolic execution.
 * @param val - Value to be printed.
 */
void crete_debug_print_f32(float val);

/**
 * @brief Prints the value (symbolic or concrete) at the calling point in SVM.
 * @param addr - Address of target memory.
 * @param size - Size of target memory.
 * @param name - Identity of the particular invocation. No restrictions.
 */
void crete_debug_print_buf(const void* addr, size_t size, const char* name);

/**
 * @brief Raises an assertion if value is concrete at calling point during symbolic execution.
 * @param addr - Address of target memory.
 * @param size - Size of target memory.
 * @param name - Identity of the particular invocation. No restrictions.
 */
void crete_debug_assert_is_concolic(const void* addr, size_t size, const char* name);

/**
 * @brief Monitors the target memory for changes in symbolic status - concrete -> symbolic or symbolic -> concrete.
 * @param addr - Address of target memory.
 * @param size - Size of target memory.
 * @param name - Unique identity of the particular invocation.
 *
 * An alert message will be printed at each status change.
 *
 * The monitor checks the symbolic or concrete status at every instruction.
 */
void crete_debug_monitor_concolic_status(const void* addr, size_t size, const char* name);

/**
 * @brief Monitors the target memory for changes in value.
 * @param addr - Address of target memory.
 * @param size - Size of target memory.
 * @param name - Unique identity of the particular invocation.
 *
 * An alert message will be printed at each value change.
 *
 * The monitor checks value at every instruction.
 */
void crete_debug_monitor_value(const void* addr, size_t size, const char* name);

/**
 * @brief Sets a flag for an existing monitor represented by "name".
 * @param flag - Flag to set.
 * @param name - Unique monitor identifier.
 */
void crete_debug_monitor_set_flag(enum CreteDebugFlag flag, const char* name);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CRETE_DEBUG_H
