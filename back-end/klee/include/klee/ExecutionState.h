//===-- ExecutionState.h ----------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXECUTIONSTATE_H
#define KLEE_EXECUTIONSTATE_H

#include "klee/Constraints.h"
#include "klee/Expr.h"
#include "klee/Internal/ADT/TreeStream.h"

// FIXME: We do not want to be exposing these? :(
#include "../../lib/Core/AddressSpace.h"
#include "klee/Internal/Module/KInstIterator.h"

#include <map>
#include <set>
#include <vector>

#if defined(CRETE_CONFIG)
#include "klee/util/Assignment.h"

#include "crete-replayer/qemu_rt_info.h"
#include "crete/trace_tag.h"

#include <deque>
#endif //CRETE_CONFIG

namespace klee {
  class Array;
  class CallPathNode;
  struct Cell;
  struct KFunction;
  struct KInstruction;
  class MemoryObject;
  class PTreeNode;
  struct InstructionInfo;

#if defined(CRETE_CONFIG)
typedef std::pair<MemoryObject*, ObjectState*> modi_objectPair;
#endif //CRETE_CONFIG

std::ostream &operator<<(std::ostream &os, const MemoryMap &mm);

struct StackFrame {
  KInstIterator caller;
  KFunction *kf;
  CallPathNode *callPathNode;

  std::vector<const MemoryObject*> allocas;
  Cell *locals;

  /// Minimum distance to an uncovered instruction once the function
  /// returns. This is not a good place for this but is used to
  /// quickly compute the context sensitive minimum distance to an
  /// uncovered instruction. This value is updated by the StatsTracker
  /// periodically.
  unsigned minDistToUncoveredOnReturn;

  // For vararg functions: arguments not passed via parameter are
  // stored (packed tightly) in a local (alloca) memory object. This
  // is setup to match the way the front-end generates vaarg code (it
  // does not pass vaarg through as expected). VACopy is lowered inside
  // of intrinsic lowering.
  MemoryObject *varargs;

  StackFrame(KInstIterator caller, KFunction *kf);
  StackFrame(const StackFrame &s);
  ~StackFrame();
};

class ExecutionState {
public:
  typedef std::vector<StackFrame> stack_ty;

private:
  // unsupported, use copy constructor
  ExecutionState &operator=(const ExecutionState&);
  std::map< std::string, std::string > fnAliases;

public:
  bool fakeState;
  // Are we currently underconstrained?  Hack: value is size to make fake
  // objects.
  unsigned underConstrained;
  unsigned depth;

  // pc - pointer to current instruction stream
  KInstIterator pc, prevPC;
  stack_ty stack;
  ConstraintManager constraints;
  mutable double queryCost;
  double weight;
  AddressSpace addressSpace;
  TreeOStream pathOS, symPathOS;
  unsigned instsSinceCovNew;
  bool coveredNew;

  /// Disables forking, set by user code.
  bool forkDisabled;

  std::map<const std::string*, std::set<unsigned> > coveredLines;
  PTreeNode *ptreeNode;

  /// ordered list of symbolics: used to generate test cases.
  //
  // FIXME: Move to a shared list structure (not critical).
  std::vector< std::pair<const MemoryObject*, const Array*> > symbolics;

  /// Set of used array names.  Used to avoid collisions.
  std::set<std::string> arrayNames;

  // Used by the checkpoint/rollback methods for fake objects.
  // FIXME: not freeing things on branch deletion.
  MemoryMap shadowObjects;

  unsigned incomingBBIndex;

  std::string getFnAlias(std::string fn);
  void addFnAlias(std::string old_fn, std::string new_fn);
  void removeFnAlias(std::string fn);

private:
  ExecutionState() : fakeState(false), underConstrained(0), ptreeNode(0) {}

public:
  ExecutionState(KFunction *kf);

  // XXX total hack, just used to make a state so solver can
  // use on structure
  ExecutionState(const std::vector<ref<Expr> > &assumptions);

  ExecutionState(const ExecutionState& state);

  ~ExecutionState();

  ExecutionState *branch();

  void pushFrame(KInstIterator caller, KFunction *kf);
  void popFrame();

  void addSymbolic(const MemoryObject *mo, const Array *array);
  void addConstraint(ref<Expr> e) {
    constraints.addConstraint(e);
  }

  bool merge(const ExecutionState &b);
  void dumpStack(std::ostream &out) const;

#if defined(CRETE_CONFIG)
public:
  // Count how many tb has been executed by klee in this state
  uint64_t m_qemu_tb_count;
  // The map between symbolic value and its concrete value
  Assignment concolics;
  // The MemoryObject that holds cpuState
  const MemoryObject *crete_cpu_state;

  // enable/disable forking in this state
  // TODO: xxx To be tested with crete_assume
  bool crete_fork_enabled;

  // flag to see whether the current TB is tainted by symbolic values
  bool crete_tb_tainted;
  bool crete_dbg_ta_fail;

  void pushCreteConcolic(ConcolicVariable cv);
  ConcolicVariable getFirstConcolic();
  void printCreteConolic();

  bool isSymbolics(MemoryObject *mo);
  ref<Expr> getConcreteExpr(ref<Expr> e);

  std::string crete_get_unique_name(const std::string name);

  // trace tag
  void check_trace_tag(bool &branch_taken, bool &explored_node);
  crete::creteTraceTag_ty get_trace_tag_for_tc() const;

private:
  std::deque<ConcolicVariable > creteConcolicsQueue;

  // trace tag
  uint64_t m_trace_tag_current_node_index;

  bool m_current_node_explored;
  vector<bool> m_current_node_br_taken;
  vector<bool> m_current_node_br_taken_semi_explored;
  uint64_t m_trace_tag_current_node_br_taken_index;

  // Debugging
public:
  void print_stack() const;
  void print_regs(std::string name, const MemoryObject *crete_cpuState);
#endif //CRETE_CONFIG
};

}

#endif
