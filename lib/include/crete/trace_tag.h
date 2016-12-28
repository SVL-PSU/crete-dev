#ifndef LIB_INCLUDE_CRETE_TRACE_TAG_H_
#define LIB_INCLUDE_CRETE_TRACE_TAG_H_

#include <iostream>
#include <stdint.h>
#include <vector>

#include <boost/serialization/vector.hpp>

using namespace std;

namespace crete
{

struct CreteTraceTagNode
{
    vector<bool> m_br_taken;
    uint64_t m_tb_pc;
    uint64_t m_tb_count;

    // For debugging:
    int m_last_opc;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & m_last_opc;
        ar & m_br_taken;
        ar & m_tb_pc;
        ar & m_tb_count;
    }
};

typedef vector<CreteTraceTagNode> creteTraceTag_ty;

inline bool operator==(const CreteTraceTagNode& lhs,
        const CreteTraceTagNode& rhs) {

    return (lhs.m_br_taken == rhs.m_br_taken) &&
            (lhs.m_last_opc == rhs.m_last_opc) &&
            (lhs.m_tb_pc == rhs.m_tb_pc) &&
            (lhs.m_tb_count == rhs.m_tb_count);
}

inline void print_br_taken(const vector<bool>& br_taken)
{
    cerr << " [";
    for(vector<bool>::const_iterator it = br_taken.begin();
            it != br_taken.end(); ++it) {
        cerr << *it << " ";
    }
    cerr << "]";
}
}

#endif /* LIB_INCLUDE_CRETE_TRACE_TAG_H_ */
