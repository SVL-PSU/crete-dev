#include "tc-compare.hpp"

#include <boost/program_options.hpp>

#include <string>

using namespace std;

namespace crete
{

CreteTcCompare::CreteTcCompare(int argc, char* argv[]) :
    m_ops_descr(make_options()),
    m_cwd(fs::current_path()),
    m_tc_folder(false)
{
    process_options(argc, argv);
    compare_tc();
}

po::options_description CreteTcCompare::make_options()
{
    po::options_description desc("Options");

    desc.add_options()
        ("help,h", "displays help message")
        ("reference,r", po::value<fs::path>(), "reference test case (folder)")
        ("target,t", po::value<fs::path>(), "target test case (folder)")
        ;

    return desc;
}

void CreteTcCompare::process_options(int argc, char* argv[])
{
    try
    {
        po::store(po::parse_command_line(argc, argv, m_ops_descr), m_var_map);
        po::notify(m_var_map);
    }
    catch(...)
    {
        cerr << boost::current_exception_diagnostic_information() << endl;
        BOOST_THROW_EXCEPTION(std::runtime_error("Error for parsing options!\n"));
    }

    if(m_var_map.size() == 0)
    {
        cout << "Missing arguments" << endl;
        cout << "Use '--help' for more details" << endl;
        exit(0);
    }
    if(m_var_map.count("help"))
    {
        cout << m_ops_descr << endl;
        exit(0);
    }

    if(m_var_map.count("reference") && m_var_map.count("target"))
    {
        fs::path p = m_var_map["reference"].as<fs::path>();

        if(!fs::exists(p))
        {
            BOOST_THROW_EXCEPTION(Exception() << err::file_missing(p.string()));
        }
        m_ref = p;

        p = m_var_map["target"].as<fs::path>();
        if(!fs::exists(p))
        {
            BOOST_THROW_EXCEPTION(Exception() << err::file_missing(p.string()));
        }
        m_tgt = p;

        if(fs::is_regular(m_ref))
        {
            assert(fs::is_regular(m_tgt));
        } else {
            assert(fs::is_directory(m_ref));
            assert(fs::is_directory(m_tgt));
            m_tc_folder = true;
        }
    } else {
        BOOST_THROW_EXCEPTION(std::runtime_error("Crete-tc-compare requires reference tc and target tc"));
    }
}


bool vector_char_compare(const vector<uint8_t>& lhs, const vector<uint8_t>& rhs)
{
    bool ret = true;

    if(lhs.size() != rhs.size())
    {
        fprintf(stderr, "length differ: %lu <-> %lu\n",
                lhs.size(), rhs.size());
        return false;
    }

    for(uint64_t i = 0; i < lhs.size(); ++i)
    {
        if(lhs[i] != rhs[i])
        {
            fprintf(stderr, "byte[%lu] differ: %lu <-> %lu\n",
                    i, (uint64_t)lhs[i], (uint64_t)rhs[i]);

            ret = false;
        }
    }

    return ret;
}

bool operator==(const TestCaseElement& lhs, const TestCaseElement& rhs)
{
    bool ret = true;

    if(lhs.name != rhs.name)
    {
        fprintf(stderr, "ele_name differ: %s <-> %s\n",
                lhs.name.data(), rhs.name.data());

        ret = false;
    } else if (lhs.data_size != rhs.data_size) {
        fprintf(stderr, "ele_size differ: %u <-> %u (%s)\n",
                lhs.data_size, rhs.data_size,
                lhs.name.data());
    } else if(lhs.data != rhs.data) {
        fprintf(stderr, "---ele_data differ: %s ---\n",
                lhs.name.data());
        ret = vector_char_compare(lhs.data, rhs.data);
        assert(!ret);
    }

    return ret;
}

static bool compare_tc_internal(const TestCase& t1, const TestCase& t2)
{
    bool ret = true;
    // Compare elements
    ret = (t1.get_elements() == t2.get_elements());

    // Compare trace-tag
    if(!(t1.get_traceTag_explored_nodes() == t2.get_traceTag_explored_nodes()))
    {
        fprintf(stderr, "+++ traceTag differ: explored_nodes\n");
        ret = false;
    }

    if(!(t1.get_traceTag_semi_explored_node() == t2.get_traceTag_semi_explored_node()))
    {
        fprintf(stderr, "+++ traceTag differ: semi_explored_node\n");
        ret = false;
    }

    if(!(t1.get_traceTag_new_nodes() == t2.get_traceTag_new_nodes()))
    {
        fprintf(stderr, "+++ traceTag differ: new_nodes\n");
        ret = false;
    }

    return ret;
}

void CreteTcCompare::compare_tc()
{
    vector<TestCase> ref_tcs;
    vector<TestCase> tgt_tcs;
    if(m_tc_folder)
    {
        ref_tcs = retrieve_tests_serialized(m_ref.string());
        tgt_tcs = retrieve_tests_serialized(m_tgt.string());
    } else {
        ref_tcs.push_back(retrieve_test_serialized(m_ref.string()));
        tgt_tcs.push_back(retrieve_test_serialized(m_tgt.string()));
    }

    uint64_t compare_size = ref_tcs.size() < tgt_tcs.size()?ref_tcs.size() : tgt_tcs.size();

    for(uint64_t i = 0; i < compare_size; ++i)
    {
        fprintf(stderr, "\n=================================================\n"
                "compare tc[%lu]\n", i);
        compare_tc_internal(ref_tcs[i], tgt_tcs[i]);
        fprintf(stderr, "=================================================\n");
    }

}

}

int main(int argc, char* argv[])
{
    try
    {
        crete::CreteTcCompare CreteTcCompare(argc, argv);
    }
    catch(...)
    {
        cerr << "[CRETE Replay] Exception Info: \n"
                << boost::current_exception_diagnostic_information() << endl;
        return -1;
    }

    return 0;
}
