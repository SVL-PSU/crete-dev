#ifndef BCT_REPLAY_H
#define BCT_REPLAY_H

#include <stdint.h>
#include <vector>
#include <string>
#include <map>

#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>

using namespace std;

namespace klee {
class ObjectState;
class Executor;
class ExecutionState;
class Expr;
}

class QemuRuntimeInfo;

extern QemuRuntimeInfo *g_qemu_rt_Info;

extern uint64_t g_test_case_count;

//FIXME: xxx
const uint64_t KLEE_ALLOC_RANGE_LOW  = 0x70000000;
const uint64_t KLEE_ALLOC_RANGE_HIGH = 0x7FFFFFFF;

/*****************************/
/* Functions for klee */
QemuRuntimeInfo* qemu_rt_info_initialize();
void qemu_rt_info_cleanup(QemuRuntimeInfo *qrt);

/*****************************/
/* structs and classes */

/* Stores the information of Concolic variables, mainly get from reading file
 * "dump_mo_symbolics.txt" and "concrete_inputs.bin", and will be used to construct
 *  symbolic memories in KLEE's memory model.
 */
struct ConcolicVariable
{
    string m_name;
    vector<uint8_t> m_concrete_value;
    uint64_t m_data_size;
    uint64_t m_guest_addr;
    uint64_t m_host_addr;

	ConcolicVariable(string name, vector<uint8_t> concrete_value,
			uint64_t data_size, uint64_t guest_addr, uint64_t host_addr)
    :m_name(name),
     m_concrete_value(concrete_value),
     m_data_size(data_size),
     m_guest_addr(guest_addr),
     m_host_addr(host_addr) {}
};

typedef vector<ConcolicVariable *> concolics_ty;
typedef pair<uint64_t, vector<uint8_t> > cv_concrete_ty; //(value, value_size)

struct CPUStateElement{
    uint64_t m_offset;
    uint64_t m_size;
    string m_name;
    vector<uint8_t> m_data;

    CPUStateElement(uint64_t offset, uint64_t size, string name, vector<uint8_t> data)
    :m_offset(offset), m_size(size), m_name(name), m_data(data) {}

    // For serialization
    CPUStateElement()
    :m_offset(0), m_size(0), m_name(string()),
     m_data(vector<uint8_t>()) {}

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & m_offset;
        ar & m_size;
        ar & m_name;
        ar & m_data;
    }
};

struct QemuInterruptInfo {
    int m_intno;
    int m_is_int;
    int m_error_code;
    int m_next_eip_addend;

    QemuInterruptInfo(int intno, int is_int, int error_code, int next_eip_addend)
    :m_intno(intno), m_is_int(is_int),
     m_error_code(error_code), m_next_eip_addend(next_eip_addend) {}

    // For serialization
    QemuInterruptInfo()
        :m_intno(0), m_is_int(0), m_error_code(0), m_next_eip_addend(0) {}

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & m_intno;
        ar & m_is_int;
        ar & m_error_code;
        ar & m_next_eip_addend;
    }
};

typedef vector< pair<uint64_t, uint8_t> > memoSyncTable_ty;
typedef vector<memoSyncTable_ty> memoSyncTables_ty;

// bool: valid table or not
// vector<>: contents
typedef pair<bool, vector<CPUStateElement> > cpuStateSyncTable_ty;

typedef pair<QemuInterruptInfo, bool> interruptState_ty;

class QemuRuntimeInfo {
private:
	// The sequence of concolic variables here is the same as how they
	// were made as symbolic by calling crete_make_concolic in qemu
	concolics_ty m_concolics;

	vector<uint8_t> m_initial_cpuState;
    vector<cpuStateSyncTable_ty> m_cpuStateSyncTables;
	memoSyncTables_ty m_memoSyncTables;

	// For Streaming Tracing
    uint64_t m_streamed_tb_count;
    uint64_t m_streamed_index;

    // For Debugging Purpose:
    // The CPUState after each interested TB being executed for cross checking on klee side
    vector<cpuStateSyncTable_ty> m_debug_cpuStateSyncTables;
	// Interrupt State Info dumped from QEMU
	vector< interruptState_ty > m_interruptStates;

public:
	QemuRuntimeInfo();
	~QemuRuntimeInfo();

	concolics_ty get_concolics() const;
	vector<uint8_t> get_initial_cpuState();

	void sync_cpuState(klee::ObjectState *wos, uint64_t tb_index);
	void cross_check_cpuState(klee::ExecutionState &state,
	        klee::ObjectState *wos, uint64_t tb_index);

	const memoSyncTable_ty& get_memoSyncTable(uint64_t tb_index);

	// For Debugging
	QemuInterruptInfo get_qemuInterruptInfo(uint64_t tb_index);
	void printMemoSyncTable(uint64_t tb_index);

	void update_qemu_CPUState(klee::ObjectState *wos,
	        uint64_t tb_index)
	__attribute__ ((deprecated));
	void verify_CpuSate_offset(string name, uint64_t offset, uint64_t size);

private:
	//TODO: xxx not a good solution
	void check_file_symbolics();

	void cleanup_concolics();

	void read_streamed_trace();
	uint32_t read_cpuSyncTables();
	uint32_t read_debug_cpuSyncTables();
	uint32_t read_memoSyncTables();
	void read_debug_cpuState_offsets();

	void init_interruptStates();
	//    uint32_t read_interruptStates();

	void init_concolics();
	void init_initial_cpuState();

	// Debugging
	void init_debug_cpuOffsetTable();
	void print_memoSyncTables();
	void print_cpuSyncTable(uint64_t tb_index) const;

public:
	// <offset, <name, size> >
	map<uint64_t, pair<string, uint64_t> >m_debug_cpuOffsetTable;

    map<string, pair<uint64_t, uint64_t> > m_debug_cpuState_offsets;
};
#endif
