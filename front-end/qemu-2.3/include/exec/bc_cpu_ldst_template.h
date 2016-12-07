/*
 *  Software MMU support
 *
 * Generate inline load/store functions for one MMU mode and data
 * size.
 *
 * Generate a store function as well as signed and unsigned loads.
 *
 * Not used directly but included from cpu_ldst.h.
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#if DATA_SIZE == 8
#define SUFFIX q
#define USUFFIX q
#define DATA_TYPE uint64_t
#elif DATA_SIZE == 4
#define SUFFIX l
#define USUFFIX l
#define DATA_TYPE uint32_t
#elif DATA_SIZE == 2
#define SUFFIX w
#define USUFFIX uw
#define DATA_TYPE uint16_t
#define DATA_STYPE int16_t
#elif DATA_SIZE == 1
#define SUFFIX b
#define USUFFIX ub
#define DATA_TYPE uint8_t
#define DATA_STYPE int8_t
#else
#error unsupported data size
#endif

#if DATA_SIZE == 8
#define RES_TYPE uint64_t
#else
#define RES_TYPE uint32_t
#endif

#ifdef SOFTMMU_CODE_ACCESS
#define ADDR_READ addr_code
#define MMUSUFFIX _cmmu
#else
#define ADDR_READ addr_read
#define MMUSUFFIX _mmu
#endif

/* generic load/store macros */
void crete_bc_assert(int valid, const char * msg);

static inline RES_TYPE
glue(glue(cpu_ld, USUFFIX), MEMSUFFIX)(CPUArchState *env, target_ulong ptr)
{
    RES_TYPE res;
    target_ulong addr;
    int mmu_idx;

    addr = ptr;
    mmu_idx = CPU_MMU_INDEX;

#if defined(CRETE_BC_MEMOP_USER)
    crete_bc_assert(mmu_idx == MMU_USER_IDX,
            "[CRETE ERROR] Helper function is not accessing userspace data!\n");
#elif defined(CRETE_BC_MEMOP_KERNEL)
    crete_bc_assert(mmu_idx != MMU_USER_IDX,
            "[CRETE ERROR] Helper function is not accessing kernel data!\n");
#else
    crete_bc_assert(0, "[CRETE ERROR] Unexpected helper function being used.\n");
#endif

    res = glue(glue(helper_ld, SUFFIX), MMUSUFFIX)(env, addr, mmu_idx);

    return res;
}

#if DATA_SIZE <= 2
static inline int
glue(glue(cpu_lds, SUFFIX), MEMSUFFIX)(CPUArchState *env, target_ulong ptr)
{
    int res;
    target_ulong addr;
    int mmu_idx;

    addr = ptr;
    mmu_idx = CPU_MMU_INDEX;

    res = (DATA_STYPE)glue(glue(helper_ld, SUFFIX),
                           MMUSUFFIX)(env, addr, mmu_idx);

#if defined(CRETE_BC_MEMOP_USER)
    crete_bc_assert(mmu_idx == MMU_USER_IDX,
            "[CRETE ERROR] Helper function is not accessing userspace data!\n");
#elif defined(CRETE_BC_MEMOP_KERNEL)
    crete_bc_assert(mmu_idx != MMU_USER_IDX,
            "[CRETE ERROR] Helper function is not accessing kernel data!\n");
#else
    crete_bc_assert(0, "[CRETE ERROR] Unexpected helper function being used.\n");
#endif

    return res;
}
#endif

#ifndef SOFTMMU_CODE_ACCESS

/* generic store macro */

static inline void
glue(glue(cpu_st, SUFFIX), MEMSUFFIX)(CPUArchState *env, target_ulong ptr,
                                      RES_TYPE v)
{
    target_ulong addr;
    int mmu_idx;

    addr = ptr;
    mmu_idx = CPU_MMU_INDEX;

#if defined(CRETE_BC_MEMOP_USER)
    crete_bc_assert(mmu_idx == MMU_USER_IDX,
            "[CRETE ERROR] Helper function is not accessing userspace data!\n");
#elif defined(CRETE_BC_MEMOP_KERNEL)
    crete_bc_assert(mmu_idx != MMU_USER_IDX,
            "[CRETE ERROR] Helper function is not accessing kernel data!\n");
#else
    crete_bc_assert(0, "[CRETE ERROR] Unexpected helper function being used.\n");
#endif

    glue(glue(helper_st, SUFFIX), MMUSUFFIX)(env, addr, v, mmu_idx);
}

#endif /* !SOFTMMU_CODE_ACCESS */

#undef RES_TYPE
#undef DATA_TYPE
#undef DATA_STYPE
#undef SUFFIX
#undef USUFFIX
#undef DATA_SIZE
#undef MMUSUFFIX
#undef ADDR_READ
