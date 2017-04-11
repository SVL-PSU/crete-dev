#include <crete/test_case.h>
#include <crete/exception.h>
#include <crete/util/util.h>

#include <external/alphanum.hpp>

#include <boost/filesystem/fstream.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>

#include <cassert>
#include <iomanip>

using namespace std;

namespace crete
{
    ostream& operator<<(ostream& os, const TestCaseElement& elem)
    {
        os << "name.size: " << elem.name_size << endl;
        os << "name: " << string(elem.name.begin(), elem.name.end()) << endl;
        os << "data.size: " << elem.data_size << endl;
        os << "data: " << string(elem.data.begin(), elem.data.end()) << endl;
        os << "hex:" << hex;
        for(vector<uint8_t>::const_iterator iter = elem.data.begin();
            iter != elem.data.end();
            ++iter)
            os << " " << setw(2) << setfill('0') << uint32_t(*iter);
        os << dec << endl;

        return os;
    }

    ostream& operator<<(ostream& os, const crete::TestCase& tc)
    {
        for(vector<TestCaseElement>::const_iterator iter = tc.elems_.begin(); iter != tc.elems_.end(); ++iter)
        {
            os << "element #" << distance(tc.elems_.begin(), iter) << endl;
            os << *iter;
        }

        return os;
    }

    void write(ostream& os, const TestCaseElement& elem)
    {
        os.write(reinterpret_cast<const char*>(&elem.name_size), sizeof(uint32_t));
        copy(elem.name.begin(), elem.name.end(), ostream_iterator<uint8_t>(os));
        os.write(reinterpret_cast<const char*>(&elem.data_size), sizeof(uint32_t));
        copy(elem.data.begin(), elem.data.end(), ostream_iterator<uint8_t>(os));
    }

    void write(ostream& os, const vector<TestCaseElement>& elems)
    {
        uint32_t elem_count = elems.size();
        os.write(reinterpret_cast<const char*>(&elem_count), sizeof(uint32_t));
        for(vector<TestCaseElement>::const_iterator iter = elems.begin(); iter != elems.end(); ++iter)
        {
            crete::write(os, *iter);
        }
    }


    void write(ostream& os, const vector<TestCase>& tcs)
    {
        uint32_t tc_count = tcs.size();
        os.write(reinterpret_cast<const char*>(&tc_count), sizeof(uint32_t));
        for(vector<TestCase>::const_iterator iter = tcs.begin(); iter != tcs.end(); ++iter)
        {
            iter->write(os);
        }
    }

    TestCase generate_complete_tc_from_patch(const TestCase& patch, const TestCase& base)
    {
        if(!patch.m_patch)
            return patch;

        // Sanity check
        assert(patch.m_base_tc_issue_index == base.m_issue_index);

        assert(patch.m_patch);
        assert(!patch.m_tcp_elems.empty());
        assert(patch.elems_.empty());
        assert(patch.m_explored_nodes.empty());
        assert(patch.m_new_nodes.empty());
        assert(patch.m_semi_explored_node.empty());

        assert(!base.m_patch);
        assert(!base.m_tcp_tt.first && !base.m_tcp_tt.second);
        assert(base.m_tcp_elems.empty());

        TestCase ret(base);
        ret.m_issue_index = patch.m_issue_index;
        ret.m_base_tc_issue_index = patch.m_base_tc_issue_index;

        // Apply patch for elems
        assert(patch.m_tcp_elems.size() == ret.elems_.size());
        for(uint64_t i = 0; i < patch.m_tcp_elems.size(); ++i)
        {
            for(TestCasePatchElement_ty::const_iterator it = patch.m_tcp_elems[i].begin();
                    it != patch.m_tcp_elems[i].end(); ++it)
            {
                uint32_t index = it->first;
                uint8_t  value = it->second;
                assert(index < ret.elems_[i].data.size());

                ret.elems_[i].data[index] = value;
            }
        }

        // Apply patch for trace-tag
        uint32_t negate_tt_index = patch.m_tcp_tt.first;
        uint32_t negate_tt_node_br_index = patch.m_tcp_tt.second;

        // For semi-explored case
        if(negate_tt_index == (ret.m_explored_nodes.size() - 1))
        {
            vector<bool>& last_node_br_taken = ret.m_explored_nodes.back().m_br_taken;
            const vector<bool>& semi_explored_br_taken = ret.m_semi_explored_node.front().m_br_taken;

            assert(!ret.m_semi_explored_node.empty());
            assert(negate_tt_node_br_index >= last_node_br_taken.size());
            assert(negate_tt_node_br_index < (last_node_br_taken.size() + semi_explored_br_taken.size()));

            last_node_br_taken.insert(last_node_br_taken.end(),
                    semi_explored_br_taken.begin(),
                    semi_explored_br_taken.begin() +
                    (negate_tt_node_br_index - last_node_br_taken.size()) + 1);

            last_node_br_taken.back() = !last_node_br_taken.back();
        } else {
            assert(negate_tt_index >=  ret.m_explored_nodes.size());
            assert(negate_tt_index < (ret.m_explored_nodes.size() + ret.m_new_nodes.size()) );

            ret.m_explored_nodes.insert(ret.m_explored_nodes.end(),
                    ret.m_new_nodes.begin(),
                    ret.m_new_nodes.begin() + (negate_tt_index - ret.m_explored_nodes.size()) + 1);

            vector<bool>& last_node_br_taken = ret.m_explored_nodes.back().m_br_taken;
            assert(negate_tt_node_br_index < last_node_br_taken.size());
            last_node_br_taken.resize(negate_tt_node_br_index + 1);
            last_node_br_taken.back() = !last_node_br_taken.back();
        }

        assert(ret.m_explored_nodes.size() == (negate_tt_index + 1) );
        assert(ret.m_explored_nodes.back().m_br_taken.size() == (negate_tt_node_br_index + 1) );
        ret.m_semi_explored_node.clear();
        ret.m_new_nodes.clear();

        return ret;
    }

    void TestCaseElement::print() const
    {
        for(uint64_t i = 0; i < name.size(); ++i)
        {
            fprintf(stderr, "%c", name[i]);
        }
        fprintf(stderr, ": %u bytes [", data_size);
        for(uint64_t i = 0; i < data.size(); ++i)
        {
            fprintf(stderr, "%u ", (uint32_t)data[i]);
        }

        fprintf(stderr, "]\n");
    }

    TestCase::TestCase() :
        priority_(0),
        m_patch(false),
        m_issue_index(0),
        m_base_tc_issue_index(0)
    {
    }

    TestCase::TestCase(const crete::TestCasePatchTraceTag_ty& tcp_tt,
            const std::vector<crete::TestCasePatchElement_ty>& tcp_elems,
            const TestCaseIssueIndex& base_tc_issue_index)
    :priority_(0),
     m_patch(true),
     m_tcp_tt(tcp_tt),
     m_tcp_elems(tcp_elems),
     m_issue_index(0),
     m_base_tc_issue_index(base_tc_issue_index)
    {}

    TestCase::TestCase(const TestCase& tc)
    :priority_(tc.priority_),
     m_patch(tc.m_patch),
     m_issue_index(tc.m_issue_index),
     m_base_tc_issue_index(tc.m_base_tc_issue_index),
     m_tcp_tt(tc.m_tcp_tt),
     m_tcp_elems(tc.m_tcp_elems),
     elems_(tc.elems_),
     m_explored_nodes(tc.m_explored_nodes),
     m_semi_explored_node(tc.m_semi_explored_node),
     m_new_nodes(tc.m_new_nodes)
    {}


    void TestCase::write(ostream& os) const
    {
        crete::write(os, elems_);
    }

    void TestCase::set_traceTag(const creteTraceTag_ty &explored_nodes,
            const creteTraceTag_ty &semi_explored_node, const creteTraceTag_ty &new_nodes)
    {
        // TODO: XXX check the input explored_nodes is consistent with m_explored_nodes
        m_explored_nodes = explored_nodes;
        m_semi_explored_node = semi_explored_node;
        m_new_nodes = new_nodes;
    }

    uint32_t TestCase::get_tt_last_node_index() const
    {
        assert(m_semi_explored_node.empty());
        assert(m_new_nodes.empty());

        if(m_patch)
        {
            // +1, as size() starts at 1, while index starts at 0
            return (m_tcp_tt.first + 1);
        } else {
            return m_explored_nodes.size();
        }
    }

    void TestCase::assert_issued_tc() const
    {
        bool valid_issue_index = (m_issue_index != 0);

        bool valid_tcp_elems = m_tcp_elems.empty();
        bool valid_m_semi_explored_node = m_semi_explored_node.empty();
        bool valid_m_new_nodes = m_new_nodes.empty();

        if(!(!m_patch && valid_issue_index &&
                valid_tcp_elems && valid_m_semi_explored_node && valid_m_new_nodes))
        {
            fprintf(stderr, "m_patch = %d, valid_issue_index = %d\n"
                    "valid_tcp_elems = %d, valid_m_semi_explored_node = %d, valid_m_new_nodes = %d\n" ,
                    (int)m_patch, (int)valid_issue_index,
                    (int)valid_tcp_elems, (int)valid_m_semi_explored_node, (int)valid_m_new_nodes);
            assert(0);
        }
    }

    TestCaseIssueIndex TestCase::get_base_tc_issue_index() const
    {
        assert(m_base_tc_issue_index != 0);
        return m_base_tc_issue_index;
    }

    TestCaseIssueIndex TestCase::get_issue_index() const
    {
        assert(m_issue_index != 0);
        return m_issue_index;
    }

    void TestCase::set_issue_index(TestCaseIssueIndex index)
    {
        assert(m_issue_index == 0);
        assert(index != 0);

        m_issue_index = index;
    }

    bool TestCase::is_test_patch() const
    {
        return m_patch;
    }

    void TestCase::assert_tc_patch() const
    {
        bool valid_issue_index = (m_issue_index == 0);
        bool valid_base_tc_issue_index= (m_base_tc_issue_index != 0);
        bool valid_tcp_elems = (!m_tcp_elems.empty());
        bool valid_elems = elems_.empty();
        bool valid_m_explored_nodes = m_explored_nodes.empty();
        bool valid_m_semi_explored_node = m_semi_explored_node.empty();
        bool valid_m_new_nodes = m_new_nodes.empty();

        if(!(m_patch && valid_issue_index && valid_base_tc_issue_index &&
                valid_tcp_elems && valid_elems && valid_m_explored_nodes &&
                valid_m_semi_explored_node && valid_m_new_nodes))
        {
            fprintf(stderr, "m_patch = %d, valid_issue_index = %d, valid_base_tc_issue_index = %d\n"
                    "valid_tcp_elems = %d, valid_elems = %d, valid_m_explored_nodes = %d\n"
                    "valid_m_semi_explored_node = %d, valid_m_new_nodes = %d\n" ,
                    (int)m_patch, (int)valid_issue_index, (int)valid_base_tc_issue_index,
                    (int)valid_tcp_elems, (int)valid_elems, (int)valid_m_explored_nodes,
                    (int)valid_m_semi_explored_node, (int)valid_m_new_nodes);
            assert(0);
        }
    }

    void TestCase::print() const
    {
        if(m_patch)
        {
            fprintf(stderr, "patch test case patch\n");
            fprintf(stderr, "m_issue_index = %lu, m_base_tc_issue_index = %lu\n"
                    "m_tcp_tt=(%u, %u), m_tcp_elems.size() = %lu:\n",
                    m_issue_index, m_base_tc_issue_index,
                    m_tcp_tt.first, m_tcp_tt.second, m_tcp_elems.size());

            for(uint64_t i = 0; i < m_tcp_elems.size(); ++i) {
                fprintf(stderr, "elm[%lu]: ", i);
                for(uint64_t j = 0; j < m_tcp_elems[i].size(); ++j)
                {
                    fprintf(stderr, "(%u, 0x%x) ", m_tcp_elems[i][j].first, (uint32_t)m_tcp_elems[i][j].second);
                }
                fprintf(stderr, "\n");
            }
        } else {
            fprintf(stderr, "complete test case (m_issue_index = %lu, m_base_tc_issue_index = %lu)\n",
                    m_issue_index, m_base_tc_issue_index);

            fprintf(stderr, "m_tcp_tt(%u, %u), m_tcp_elems.size() = %lu\n"
                    "m_explored_nodes.size() = %lu, m_semi_explored_node.size()=%lu, m_new_nodes.size() = %lu\n"
                    "elems_.size() = %lu\n",
                    m_tcp_tt.first, m_tcp_tt.second, m_tcp_elems.size(),
                    m_explored_nodes.size(), m_semi_explored_node.size(), m_new_nodes.size(),
                    elems_.size());

            for(uint64_t i = 0; i <  elems_.size(); ++i)
            {
                fprintf(stderr, "elems_[%lu]: ", i);
                elems_[i].print();
            }
        }
    }

    TestCaseElement read_test_case_element(istream& is)
    {
        TestCaseElement elem;

        istream::pos_type tsize = util::stream_size(is);

        CRETE_EXCEPTION_ASSERT(tsize >= 0, err::signed_to_unsigned_conversion(tsize));

        uint32_t ssize = static_cast<uint32_t>(tsize);

        if(ssize > 1000000)
        {
            BOOST_THROW_EXCEPTION(Exception() << err::msg("Sanity check: test case file size is unexpectedly large (size > 1000000)."));
        }

        is.read(reinterpret_cast<char*>(&elem.name_size), sizeof(uint32_t));
        assert(elem.name_size < ssize);

        if(elem.name_size > 1000)
        {
             BOOST_THROW_EXCEPTION(Exception() << err::msg("Sanity check: test case element name size is unexpectedly large (size > 1000)."));
        }

        elem.name.resize(elem.name_size);
        is.read(reinterpret_cast<char*>(elem.name.data()), elem.name_size);


        is.read(reinterpret_cast<char*>(&elem.data_size), sizeof(uint32_t));
        assert(elem.data_size < ssize);

        if(elem.data_size > 1000000)
        {
             BOOST_THROW_EXCEPTION(Exception() << err::msg("Sanity check: test case element name size is unexpectedly large (size > 1000000)."));
        }

        elem.data.resize(elem.data_size); // TODO: inefficient. Resize initializes all values.
        is.read(reinterpret_cast<char*>(elem.data.data()), elem.data_size);

        return elem;
    }

    TestCase read_test_case(istream& is)
    {
        TestCase tc;

        istream::pos_type tsize = util::stream_size(is);

        CRETE_EXCEPTION_ASSERT(tsize >= 0, err::signed_to_unsigned_conversion(tsize));

        uint32_t ssize = static_cast<uint32_t>(tsize);

        if(ssize > 1000000)
        {
            BOOST_THROW_EXCEPTION(Exception() << err::msg("Sanity check: test case file size is unexpectedly large (size > 1000000)."));
        }

        uint32_t elem_count;
        is.read(reinterpret_cast<char*>(&elem_count), sizeof(uint32_t));

        assert(elem_count != 0 &&
               elem_count < (ssize / elem_count));

        for(uint32_t i = 0; i < elem_count; ++i)
        {
            tc.add_element(read_test_case_element(is));
        }

        return tc;
    }

    vector<TestCase> read_test_cases(istream& is)
    {
        vector<TestCase> tcs;

        istream::pos_type tsize = util::stream_size(is);

        CRETE_EXCEPTION_ASSERT(tsize >= 0, err::signed_to_unsigned_conversion(tsize));

        uint32_t ssize = static_cast<uint32_t>(tsize);

        uint32_t tc_count;
        is.read(reinterpret_cast<char*>(&tc_count), sizeof(uint32_t));

        assert(tc_count != 0 &&
               tc_count < (ssize / tc_count));

        for(uint32_t i = 0; i < tc_count; ++i)
        {
            tcs.push_back(read_test_case(is));
        }

        return tcs;
    }

    bool empty_test_case(istream& is) {
        uint32_t elem_count = element_count_test_case(is);
        return (elem_count == 0)? true:false;
    }

    uint32_t element_count_test_case(istream& is){
        uint32_t elem_count;
        is.read(reinterpret_cast<char*>(&elem_count), sizeof(uint32_t));
        return elem_count;
    }

    vector<TestCase> retrieve_tests(const string& tc_dir)
    {
        namespace fs = boost::filesystem;

        const fs::path test_pool_dir(tc_dir);
        CRETE_EXCEPTION_ASSERT(fs::exists(test_pool_dir),
                err::file_missing(test_pool_dir.string()));
        assert(fs::is_directory(test_pool_dir));

        // Sort the files alphabetically
        vector<string> v;
        for ( fs::directory_iterator itr( test_pool_dir );
              itr != fs::directory_iterator();
              ++itr ){
            v.push_back(itr->path().string());
        }

        sort(v.begin(), v.end(), doj::alphanum_less<string>());
//        sort(v.begin(), v.end());
        vector<TestCase> tests;

        for (vector<string>::const_iterator it(v.begin()), it_end(v.end());
                it != it_end; ++it)
        {
            fs::path entry(*it);

            // Test case with no elements
            // TODO: xxx report after retrieve all the valid test cases
            if(fs::file_size(entry) <= 4)
            {
                BOOST_THROW_EXCEPTION(Exception() << err::msg("Invalid test case: empty elements."));
            }

            fs::ifstream tests_file(entry);
            CRETE_EXCEPTION_ASSERT(tests_file.good(), err::file_open_failed(entry.string()));

            tests.push_back(read_test_case(tests_file));
        }

        return tests;
    }

    TestCase retrieve_test(const std::string& tc_path)
    {
        namespace fs = boost::filesystem;
        fs::path entry(tc_path);
        CRETE_EXCEPTION_ASSERT(fs::file_size(entry) >= 4, err::file(entry.string()));

        fs::ifstream tests_file(entry);
        CRETE_EXCEPTION_ASSERT(tests_file.good(), err::file_open_failed(entry.string()));

        return read_test_case(tests_file);
    }

    void write_serialized(ostream& os, const TestCase& tc)
    {
        try {
            boost::archive::binary_oarchive oa(os);
            oa << tc;
        }
        catch(std::exception &e){
            BOOST_THROW_EXCEPTION(Exception() << err::msg("Serialization error in write_serialized()\n"));
        };
    }

    TestCase read_serialized(istream& is)
    {
        try {
            TestCase tc;
            boost::archive::binary_iarchive ia(is);
            ia >> tc;

            return tc;
        }
        catch(std::exception &e){
            BOOST_THROW_EXCEPTION(Exception() << err::msg("Serialization error in read_serialized()\n"));
        };
    }

    TestCase retrieve_test_serialized(const std::string& tc_path)
    {
        namespace fs = boost::filesystem;
        fs::ifstream tests_file(tc_path);
        CRETE_EXCEPTION_ASSERT(tests_file.good(), err::file_open_failed(tc_path));

        return read_serialized(tests_file);
    }

    // Return empty vector if the folder does not exist or is empty
    vector<TestCase> retrieve_tests_serialized(const string& tc_dir)
    {
        namespace fs = boost::filesystem;

        const fs::path test_pool_dir(tc_dir);

        if(!fs::exists(test_pool_dir))
        {
            return vector<TestCase>();
        }

        assert(fs::is_directory(test_pool_dir));

        // Sort the files alphabetically
        vector<string> v;
        for ( fs::directory_iterator itr( test_pool_dir );
              itr != fs::directory_iterator();
              ++itr ){
            v.push_back(itr->path().string());
        }

        sort(v.begin(), v.end(), doj::alphanum_less<string>());
//        sort(v.begin(), v.end());
        vector<TestCase> tests;

        for (vector<string>::const_iterator it(v.begin()), it_end(v.end());
                it != it_end; ++it)
        {
            fs::path entry(*it);

            fs::ifstream tests_file(entry);
            CRETE_EXCEPTION_ASSERT(tests_file.good(), err::file_open_failed(entry.string()));

            tests.push_back(read_serialized(tests_file));
        }

        return tests;
    }
}
