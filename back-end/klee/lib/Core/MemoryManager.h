//===-- MemoryManager.h -----------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_MEMORYMANAGER_H
#define KLEE_MEMORYMANAGER_H

#include <set>
#include <stdint.h>

namespace llvm {
  class Value;
}

namespace klee {
  class MemoryObject;

  class MemoryManager {
  private:
    typedef std::set<MemoryObject*> objects_ty;
    objects_ty objects;

  public:
    MemoryManager();
    ~MemoryManager();

    MemoryObject *allocate(uint64_t size, bool isLocal, bool isGlobal,
                           const llvm::Value *allocSite);
    MemoryObject *allocateFixed(uint64_t address, uint64_t size,
                                const llvm::Value *allocSite,
                                bool crete_call = false);
    void deallocate(const MemoryObject *mo);
    void markFreed(MemoryObject *mo);

#if defined(CRETE_CONFIG)
  public:
    // TODO: xxx move them to class AddressSpace?
    MemoryObject *findObject(uint64_t address) const;
    std::vector<MemoryObject *> findOverlapObjects(uint64_t address, uint64_t size) const;
    bool isOverlappedMO(uint64_t address, uint64_t size) const;

  private:
    uint64_t next_alloc_address;

    uint64_t get_next_address(uint64_t size);
#endif
  };
} // End klee namespace

#endif
