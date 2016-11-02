#include "config.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "exec/bc_cpu_ldst.h"

#include "exec/cputlb.h"

#include "exec/memory-internal.h"
#include "exec/ram_addr.h"
#include "tcg/tcg.h"

#define MMUSUFFIX _mmu

#define SHIFT 0
#include "bc_softmmu_template.h"

#define SHIFT 1
#include "bc_softmmu_template.h"

#define SHIFT 2
#include "bc_softmmu_template.h"

#define SHIFT 3
#include "bc_softmmu_template.h"
#undef MMUSUFFIX
