//===-- MemoryManager.cpp -------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Common.h"

#include "CoreStats.h"
#include "Memory.h"
#include "MemoryManager.h"

#include "klee/ExecutionState.h"
#include "klee/Expr.h"
#include "klee/Solver.h"

#include "llvm/Support/CommandLine.h"

#if defined(CRETE_CONFIG)
#include "crete-replayer/qemu_rt_info.h"
#endif // CRETE_CONFIG

using namespace klee;

/***/
MemoryManager::MemoryManager(){
#if defined(CRETE_CONFIG)
    next_alloc_address = 0;
#endif
}

MemoryManager::~MemoryManager() {
  while (!objects.empty()) {
    MemoryObject *mo = *objects.begin();
    if (!mo->isFixed)
      free((void *)mo->address);
    objects.erase(mo);
    delete mo;
  }
}

#if !defined(CRETE_CONFIG)
MemoryObject *MemoryManager::allocate(uint64_t size, bool isLocal,
                                      bool isGlobal,
                                      const llvm::Value *allocSite) {
  if (size>10*1024*1024)
    klee_warning_once(0, "Large alloc: %u bytes.  KLEE may run out of memory.", (unsigned) size);

  uint64_t address = (uint64_t) (unsigned long) malloc((unsigned) size);
  if (!address)
    return 0;

  ++stats::allocations;
  MemoryObject *res = new MemoryObject(address, size, isLocal, isGlobal, false,
                                       allocSite, this);
  objects.insert(res);
  return res;
}
#else //CRETE_CONFIG
MemoryObject *MemoryManager::allocate(uint64_t size, bool isLocal,
                                      bool isGlobal,
                                      const llvm::Value *allocSite) {
    uint64_t address = get_next_address(size);

    ++stats::allocations;
    bool isFixed = true;
    MemoryObject *res = new MemoryObject(address, size, isLocal, isGlobal, isFixed,
            allocSite, this);
    objects.insert(res);

    return res;
}
#endif // CRETE_CONFIG

MemoryObject *MemoryManager::allocateFixed(uint64_t address, uint64_t size,
                                           const llvm::Value *allocSite,
                                           bool crete_call) {
#ifndef NDEBUG
  for (objects_ty::iterator it = objects.begin(), ie = objects.end();
       it != ie; ++it) {
    MemoryObject *mo = *it;
    if (address+size > mo->address && address < mo->address+mo->size) {
        // Check on the validity of return is done by crete code;
        if(crete_call){
            cerr << "[CRETE ERROR] Trying to allocate an overlapping object: "
                    << "addr = 0x"<< hex << mo->address
                    << ", size = " << dec << mo->size << endl;
            return NULL;
        }

        klee_error("Trying to allocate an overlapping object");
    }
  }
#endif

  ++stats::allocations;
  MemoryObject *res = new MemoryObject(address, size, false, true, true,
                                       allocSite, this);
  objects.insert(res);
  return res;
}

void MemoryManager::deallocate(const MemoryObject *mo) {
  assert(0);
}

void MemoryManager::markFreed(MemoryObject *mo) {
  if (objects.find(mo) != objects.end())
  {
    if (!mo->isFixed)
      free((void *)mo->address);
    objects.erase(mo);
  }
}

#if defined(CRETE_CONFIG)
MemoryObject *MemoryManager::findObject(uint64_t address) const {
  for (objects_ty::iterator it = objects.begin(), ie = objects.end();
       it != ie; ++it) {
    MemoryObject *mo = *it;
    if (address == mo->address )
      return mo;
  }

  return 0;
}

std::vector<MemoryObject *> MemoryManager::findOverlapObjects(uint64_t address,
        uint64_t size) const {
    std::vector<MemoryObject *> overlapped_mos;

    for (objects_ty::iterator it = objects.begin(), ie = objects.end();
            it != ie; ++it) {
        MemoryObject *mo = *it;
        if (address+size > mo->address && address < mo->address+mo->size) {
            overlapped_mos.push_back(mo);
        }
    }

    return overlapped_mos;
}

bool MemoryManager::isOverlappedMO(uint64_t address, uint64_t size) const {
    for (objects_ty::iterator it = objects.begin(), ie = objects.end();
            it != ie; ++it) {
        MemoryObject *mo = *it;
        if (address+size > mo->address && address < mo->address+mo->size) {
            return true;
        }
    }

    return false;
}

uint64_t MemoryManager::get_next_address(uint64_t size) {
  if (next_alloc_address < KLEE_ALLOC_RANGE_LOW) {
    next_alloc_address = KLEE_ALLOC_RANGE_LOW;
  }

  assert(next_alloc_address + size <= KLEE_ALLOC_RANGE_HIGH && "[MemoryManager::get_next_address] allocate overflow.");

  uint64_t ret = next_alloc_address;
  next_alloc_address += size;

  return ret;
}
#endif // CRETE_CONFIG
