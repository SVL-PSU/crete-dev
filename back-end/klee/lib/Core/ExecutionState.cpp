//===-- ExecutionState.cpp ------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/ExecutionState.h"

#include "klee/Internal/Module/Cell.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/KModule.h"

#include "klee/Expr.h"

#include "Memory.h"
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 3)
#include "llvm/IR/Function.h"
#else
#include "llvm/Function.h"
#endif
#include "llvm/Support/CommandLine.h"

#include <iostream>
#include <iomanip>
#include <cassert>
#include <map>
#include <set>
#include <stdarg.h>

using namespace llvm;
using namespace klee;

namespace {
  cl::opt<bool>
  DebugLogStateMerge("debug-log-state-merge");
}

/***/

StackFrame::StackFrame(KInstIterator _caller, KFunction *_kf)
  : caller(_caller), kf(_kf), callPathNode(0),
    minDistToUncoveredOnReturn(0), varargs(0) {
  locals = new Cell[kf->numRegisters];
}

StackFrame::StackFrame(const StackFrame &s)
  : caller(s.caller),
    kf(s.kf),
    callPathNode(s.callPathNode),
    allocas(s.allocas),
    minDistToUncoveredOnReturn(s.minDistToUncoveredOnReturn),
    varargs(s.varargs) {
  locals = new Cell[s.kf->numRegisters];
  for (unsigned i=0; i<s.kf->numRegisters; i++)
    locals[i] = s.locals[i];
}

StackFrame::~StackFrame() {
  delete[] locals;
}

/***/

ExecutionState::ExecutionState(KFunction *kf)
  : fakeState(false),
    underConstrained(false),
    depth(0),
    pc(kf->instructions),
    prevPC(pc),
    queryCost(0.),
    weight(1),
    instsSinceCovNew(0),
    coveredNew(false),
    forkDisabled(false),
    ptreeNode(0)
#ifdef CRETE_CONFIG
    ,m_qemu_tb_count(0),
    concolics(true),
    crete_cpu_state(NULL),
	crete_fork_enabled(true),
	crete_tb_tainted(false),
	crete_dbg_ta_fail(false)
#endif
{
  pushFrame(0, kf);
}

ExecutionState::ExecutionState(const std::vector<ref<Expr> > &assumptions)
  : fakeState(true),
    underConstrained(false),
    constraints(assumptions),
    queryCost(0.),
    ptreeNode(0)
#ifdef CRETE_CONFIG
    ,m_qemu_tb_count(0),
    concolics(true),
    crete_cpu_state(NULL),
	crete_fork_enabled(true),
	crete_tb_tainted(false),
	crete_dbg_ta_fail(false)
#endif
{}

ExecutionState::~ExecutionState() {
  for (unsigned int i=0; i<symbolics.size(); i++)
  {
    const MemoryObject *mo = symbolics[i].first;
    assert(mo->refCount > 0);
    mo->refCount--;
    if (mo->refCount == 0)
      delete mo;
  }

  while (!stack.empty()) popFrame();
}

ExecutionState::ExecutionState(const ExecutionState& state)
  : fnAliases(state.fnAliases),
    fakeState(state.fakeState),
    underConstrained(state.underConstrained),
    depth(state.depth),
    pc(state.pc),
    prevPC(state.prevPC),
    stack(state.stack),
    constraints(state.constraints),
    queryCost(state.queryCost),
    weight(state.weight),
    addressSpace(state.addressSpace),
    pathOS(state.pathOS),
    symPathOS(state.symPathOS),
    instsSinceCovNew(state.instsSinceCovNew),
    coveredNew(state.coveredNew),
    forkDisabled(state.forkDisabled),
    coveredLines(state.coveredLines),
    ptreeNode(state.ptreeNode),
    symbolics(state.symbolics),
    arrayNames(state.arrayNames),
    shadowObjects(state.shadowObjects),
    incomingBBIndex(state.incomingBBIndex)
#ifdef CRETE_CONFIG
    ,m_qemu_tb_count(state.m_qemu_tb_count),
    concolics(state.concolics),
    crete_cpu_state(state.crete_cpu_state),
	crete_fork_enabled(state.crete_fork_enabled),
	crete_tb_tainted(state.crete_tb_tainted),
	crete_dbg_ta_fail(state.crete_dbg_ta_fail)
#endif
{
  for (unsigned int i=0; i<symbolics.size(); i++)
    symbolics[i].first->refCount++;
}

ExecutionState *ExecutionState::branch() {
  depth++;

  ExecutionState *falseState = new ExecutionState(*this);
  falseState->coveredNew = false;
  falseState->coveredLines.clear();

  weight *= .5;
  falseState->weight -= weight;

  return falseState;
}

void ExecutionState::pushFrame(KInstIterator caller, KFunction *kf) {
  stack.push_back(StackFrame(caller,kf));
}

void ExecutionState::popFrame() {
  StackFrame &sf = stack.back();
  for (std::vector<const MemoryObject*>::iterator it = sf.allocas.begin(),
         ie = sf.allocas.end(); it != ie; ++it)
    addressSpace.unbindObject(*it);
  stack.pop_back();
}

void ExecutionState::addSymbolic(const MemoryObject *mo, const Array *array) {
  mo->refCount++;
  symbolics.push_back(std::make_pair(mo, array));

#if defined(CRETE_CONFIG)
  assert(0 && "[CRETE Error] makeSymbolic should be handled within Executor::handleCreteMakeSymbolic()");
#endif
}
///

std::string ExecutionState::getFnAlias(std::string fn) {
  std::map < std::string, std::string >::iterator it = fnAliases.find(fn);
  if (it != fnAliases.end())
    return it->second;
  else return "";
}

void ExecutionState::addFnAlias(std::string old_fn, std::string new_fn) {
  fnAliases[old_fn] = new_fn;
}

void ExecutionState::removeFnAlias(std::string fn) {
  fnAliases.erase(fn);
}

/**/

std::ostream &klee::operator<<(std::ostream &os, const MemoryMap &mm) {
  os << "{";
  MemoryMap::iterator it = mm.begin();
  MemoryMap::iterator ie = mm.end();
  if (it!=ie) {
    os << "MO" << it->first->id << ":" << it->second;
    for (++it; it!=ie; ++it)
      os << ", MO" << it->first->id << ":" << it->second;
  }
  os << "}";
  return os;
}

bool ExecutionState::merge(const ExecutionState &b) {
  if (DebugLogStateMerge)
    std::cerr << "-- attempting merge of A:"
               << this << " with B:" << &b << "--\n";
  if (pc != b.pc)
    return false;

  // XXX is it even possible for these to differ? does it matter? probably
  // implies difference in object states?
  if (symbolics!=b.symbolics)
    return false;

  {
    std::vector<StackFrame>::const_iterator itA = stack.begin();
    std::vector<StackFrame>::const_iterator itB = b.stack.begin();
    while (itA!=stack.end() && itB!=b.stack.end()) {
      // XXX vaargs?
      if (itA->caller!=itB->caller || itA->kf!=itB->kf)
        return false;
      ++itA;
      ++itB;
    }
    if (itA!=stack.end() || itB!=b.stack.end())
      return false;
  }

  std::set< ref<Expr> > aConstraints(constraints.begin(), constraints.end());
  std::set< ref<Expr> > bConstraints(b.constraints.begin(),
                                     b.constraints.end());
  std::set< ref<Expr> > commonConstraints, aSuffix, bSuffix;
  std::set_intersection(aConstraints.begin(), aConstraints.end(),
                        bConstraints.begin(), bConstraints.end(),
                        std::inserter(commonConstraints, commonConstraints.begin()));
  std::set_difference(aConstraints.begin(), aConstraints.end(),
                      commonConstraints.begin(), commonConstraints.end(),
                      std::inserter(aSuffix, aSuffix.end()));
  std::set_difference(bConstraints.begin(), bConstraints.end(),
                      commonConstraints.begin(), commonConstraints.end(),
                      std::inserter(bSuffix, bSuffix.end()));
  if (DebugLogStateMerge) {
    std::cerr << "\tconstraint prefix: [";
    for (std::set< ref<Expr> >::iterator it = commonConstraints.begin(),
           ie = commonConstraints.end(); it != ie; ++it)
      std::cerr << *it << ", ";
    std::cerr << "]\n";
    std::cerr << "\tA suffix: [";
    for (std::set< ref<Expr> >::iterator it = aSuffix.begin(),
           ie = aSuffix.end(); it != ie; ++it)
      std::cerr << *it << ", ";
    std::cerr << "]\n";
    std::cerr << "\tB suffix: [";
    for (std::set< ref<Expr> >::iterator it = bSuffix.begin(),
           ie = bSuffix.end(); it != ie; ++it)
      std::cerr << *it << ", ";
    std::cerr << "]\n";
  }

  // We cannot merge if addresses would resolve differently in the
  // states. This means:
  //
  // 1. Any objects created since the branch in either object must
  // have been free'd.
  //
  // 2. We cannot have free'd any pre-existing object in one state
  // and not the other

  if (DebugLogStateMerge) {
    std::cerr << "\tchecking object states\n";
    std::cerr << "A: " << addressSpace.objects << "\n";
    std::cerr << "B: " << b.addressSpace.objects << "\n";
  }

  std::set<const MemoryObject*> mutated;
  MemoryMap::iterator ai = addressSpace.objects.begin();
  MemoryMap::iterator bi = b.addressSpace.objects.begin();
  MemoryMap::iterator ae = addressSpace.objects.end();
  MemoryMap::iterator be = b.addressSpace.objects.end();
  for (; ai!=ae && bi!=be; ++ai, ++bi) {
    if (ai->first != bi->first) {
      if (DebugLogStateMerge) {
        if (ai->first < bi->first) {
          std::cerr << "\t\tB misses binding for: " << ai->first->id << "\n";
        } else {
          std::cerr << "\t\tA misses binding for: " << bi->first->id << "\n";
        }
      }
      return false;
    }
    if (ai->second != bi->second) {
      if (DebugLogStateMerge)
        std::cerr << "\t\tmutated: " << ai->first->id << "\n";
      mutated.insert(ai->first);
    }
  }
  if (ai!=ae || bi!=be) {
    if (DebugLogStateMerge)
      std::cerr << "\t\tmappings differ\n";
    return false;
  }

  // merge stack

  ref<Expr> inA = ConstantExpr::alloc(1, Expr::Bool);
  ref<Expr> inB = ConstantExpr::alloc(1, Expr::Bool);
  for (std::set< ref<Expr> >::iterator it = aSuffix.begin(),
         ie = aSuffix.end(); it != ie; ++it)
    inA = AndExpr::create(inA, *it);
  for (std::set< ref<Expr> >::iterator it = bSuffix.begin(),
         ie = bSuffix.end(); it != ie; ++it)
    inB = AndExpr::create(inB, *it);

  // XXX should we have a preference as to which predicate to use?
  // it seems like it can make a difference, even though logically
  // they must contradict each other and so inA => !inB

  std::vector<StackFrame>::iterator itA = stack.begin();
  std::vector<StackFrame>::const_iterator itB = b.stack.begin();
  for (; itA!=stack.end(); ++itA, ++itB) {
    StackFrame &af = *itA;
    const StackFrame &bf = *itB;
    for (unsigned i=0; i<af.kf->numRegisters; i++) {
      ref<Expr> &av = af.locals[i].value;
      const ref<Expr> &bv = bf.locals[i].value;
      if (av.isNull() || bv.isNull()) {
        // if one is null then by implication (we are at same pc)
        // we cannot reuse this local, so just ignore
      } else {
        av = SelectExpr::create(inA, av, bv);
      }
    }
  }

  for (std::set<const MemoryObject*>::iterator it = mutated.begin(),
         ie = mutated.end(); it != ie; ++it) {
    const MemoryObject *mo = *it;
    const ObjectState *os = addressSpace.findObject(mo);
    const ObjectState *otherOS = b.addressSpace.findObject(mo);
    assert(os && !os->readOnly &&
           "objects mutated but not writable in merging state");
    assert(otherOS);

    ObjectState *wos = addressSpace.getWriteable(mo, os);
    for (unsigned i=0; i<mo->size; i++) {
      ref<Expr> av = wos->read8(i);
      ref<Expr> bv = otherOS->read8(i);
      wos->write(i, SelectExpr::create(inA, av, bv));
    }
  }

  constraints = ConstraintManager();
  for (std::set< ref<Expr> >::iterator it = commonConstraints.begin(),
         ie = commonConstraints.end(); it != ie; ++it)
    constraints.addConstraint(*it);
  constraints.addConstraint(OrExpr::create(inA, inB));

  return true;
}

void ExecutionState::dumpStack(std::ostream &out) const {
  unsigned idx = 0;
  const KInstruction *target = prevPC;
  for (ExecutionState::stack_ty::const_reverse_iterator
         it = stack.rbegin(), ie = stack.rend();
       it != ie; ++it) {
    const StackFrame &sf = *it;
    Function *f = sf.kf->function;
    const InstructionInfo &ii = *target->info;
    out << "\t#" << idx++
        << " " << std::setw(8) << std::setfill('0') << ii.assemblyLine
        << " in " << f->getName().str() << " (";
    // Yawn, we could go up and print varargs if we wanted to.
    unsigned index = 0;
    for (Function::arg_iterator ai = f->arg_begin(), ae = f->arg_end();
         ai != ae; ++ai) {
      if (ai!=f->arg_begin()) out << ", ";

      out << ai->getName().str();
      // XXX should go through function
      ref<Expr> value = sf.locals[sf.kf->getArgRegister(index++)].value;
      if (isa<ConstantExpr>(value))
        out << "=" << value;
    }
    out << ")";
    if (ii.file != "")
      out << " at " << ii.file << ":" << ii.line;
    out << "\n";
    target = sf.caller;
  }
}

#if defined(CRETE_CONFIG)
void ExecutionState::pushCreteConcolic(ConcolicVariable cv) {
	creteConcolicsQueue.push_back(cv);
}

ConcolicVariable ExecutionState::getFirstConcolic() {
	assert(!creteConcolicsQueue.empty() &&
			"creteConcolicsQueue1 is empty, which means __crete_make_symbolic is executed more than expected!\n");

	ConcolicVariable front_cv= creteConcolicsQueue.front();
	creteConcolicsQueue.pop_front();
	return front_cv;
}

void ExecutionState::printCreteConolic(){
	std::cerr << "creteConcolicsQueue1.size() = " << std::dec
			<< creteConcolicsQueue.size() << std::endl;
	for(unsigned long i = 0; i < creteConcolicsQueue.size(); ++i) {
		std::cerr<<  creteConcolicsQueue[i].m_name << ": "
				<< "0x" << std::hex << creteConcolicsQueue[i].m_guest_addr
				<< std::dec << creteConcolicsQueue[i].m_data_size
				<< std::endl;
	}
}

bool ExecutionState::isSymbolics(MemoryObject *mo) {
	bool ret = false;
	for (unsigned int i=0; i<symbolics.size(); i++) {
		if(symbolics[i].first == mo) {
			ret = true;
			break;
		}
	}

	return ret;
}

ref<Expr> ExecutionState::getConcreteExpr(ref<Expr> e)
{
	ref<Expr> sym_expr = constraints.simplifyExpr(e);
	return concolics.evaluate(sym_expr);
}

void ExecutionState::print_stack() const
{
    cerr << "Bit-code execution stack:\n";
    uint32_t count = 0;
    for(stack_ty::const_reverse_iterator it = stack.rbegin();
            it != stack.rend(); ++it) {
        std::cerr << dec << count++ << ": "
                << it->kf->function->getName().str() <<std::endl;
    }
}

void ExecutionState::print_regs(std::string name, const MemoryObject *crete_cpuState)
{
    assert(crete_cpuState);
    const ObjectState* os = addressSpace.findObject(crete_cpuState);
    assert(os);

    cerr << "addr_crete_cpuState = 0x" << hex << crete_cpuState->address << endl;
    for(map<uint64_t, pair<std::string, uint64_t> >::const_iterator it
            = g_qemu_rt_Info->m_debug_cpuOffsetTable.begin();
            it != g_qemu_rt_Info->m_debug_cpuOffsetTable.end(); ++it) {
        if(it->second.first.find(name) != std::string::npos) {
            uint64_t offset = it->first;
            uint64_t size = it->second.second;

            vector<uint8_t> current_value;
            for(uint64_t i = 0; i < size; ++i) {
                klee::ref<klee::Expr> ref_current_value_byte;
                uint8_t current_value_byte;

                ref_current_value_byte = os->read8(offset + i);
                if(!isa<klee::ConstantExpr>(ref_current_value_byte)) {
                    ref_current_value_byte = getConcreteExpr(ref_current_value_byte);
                    assert(isa<klee::ConstantExpr>(ref_current_value_byte));
                }
                current_value_byte = (uint8_t)llvm::cast<klee::ConstantExpr>(
                        ref_current_value_byte)->getZExtValue(8);

                current_value.push_back(current_value_byte);
            }

            cerr << it->second.first << ": 0x" << hex << it->first << "[";
            for(uint64_t i = 0; i < size; ++i) {
                cerr << " 0x" << hex << (uint32_t)current_value[i];
            }
            cerr << "]\n";
        }
    }
}

std::string ExecutionState::crete_get_unique_name(const std::string name)
{
    unsigned id = 0;
    std::string uniqueName = name;
    while (!arrayNames.insert(uniqueName).second) {
      uniqueName = name + "_" + llvm::utostr(++id);
    }

    return uniqueName;
}

#endif // CRETE_CONFIG
