/*
 * guest_data_post_exec.hpp
 *
 *  Created on: Feb 7, 2017
 *      Author: chenbo
 */

#ifndef LIB_INCLUDE_CRETE_GUEST_DATA_POST_EXEC_HPP_
#define LIB_INCLUDE_CRETE_GUEST_DATA_POST_EXEC_HPP_

#include <string>
#include <stdint.h>
#include <vector>

#include <boost/filesystem/path.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/archive/binary_iarchive.hpp>

using namespace std;

const string CRETE_FILENAME_GUEST_DATA_POST_EXEC =
        "crete.guest.data.post.data.bin";
namespace crete
{
// Data needed by dispatch from vm-node
struct GuestDataPostExec
{
    vector<uint64_t> m_new_captured_tbs;

    GuestDataPostExec() {};
    ~GuestDataPostExec() {};

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & m_new_captured_tbs;
    }

    void add_new_tb_pc(const uint64_t pc)
    {
        m_new_captured_tbs.push_back(pc);
    }
};

inline GuestDataPostExec read_serialized_guest_data_post_exec(const boost::filesystem::path &input)
{
    GuestDataPostExec ret;
    ifstream i_sm(input.string().c_str(), ios_base::binary);
    assert(i_sm.good());

    boost::archive::binary_iarchive ia(i_sm);
    GuestDataPostExec temp;
    ia >> ret;

    return ret;
};

} //namespace crete


#endif /* LIB_INCLUDE_CRETE_GUEST_DATA_POST_EXEC_HPP_ */
