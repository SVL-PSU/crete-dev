#ifndef LIB_INCLUDE_CRETE_TRACE_TAG_H_
#define LIB_INCLUDE_CRETE_TRACE_TAG_H_

using namespace std;

namespace crete
{

struct CreteTraceTagNode
{
    int m_last_opc;
    bool m_br_taken;
    uint64_t m_tb_pc;

    uint64_t m_tb_count;

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

}



#endif /* LIB_INCLUDE_CRETE_TRACE_TAG_H_ */
