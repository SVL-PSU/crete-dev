#include "tc-compare.hpp"

#include <boost/program_options.hpp>
#include <boost/unordered_map.hpp>
#include <boost/lexical_cast.hpp>

#include <string>
#include <sstream>

using namespace std;

namespace crete
{

CreteTcCompare::CreteTcCompare(int argc, char* argv[]) :
    m_ops_descr(make_options()),
    m_cwd(fs::current_path()),
    m_tc_folder(false)
{
    process_options(argc, argv);

    if(!m_ref.empty())
        compare_tc();
    else if (!m_patch.empty())
        generate_complete_test_from_patch();
    else if (!m_batch_patch.empty())
        batch_path_mode();
}

po::options_description CreteTcCompare::make_options()
{
    po::options_description desc("Options");

    desc.add_options()
        ("help,h", "displays help message")
        ("reference,r", po::value<fs::path>(), "reference test case (folder)")
        ("target,t", po::value<fs::path>(), "target test case (folder)")
        ("patch,p", po::value<fs::path>(), "patch mode")
        ("batch-patch,b", po::value<fs::path>(), "input directory for patching test case in batch-patch mode")
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
    } else if (m_var_map.count("patch")) {
        fs::path p = m_var_map["patch"].as<fs::path>();

        if(!fs::exists(p))
        {
            BOOST_THROW_EXCEPTION(Exception() << err::file_missing(p.string()));
        }

        m_patch = p;

        if(fs::is_directory(m_patch))
        {
            m_tc_folder = true;
        } else {
            assert(fs::is_regular(m_patch));
        }
    }else if (m_var_map.count("batch-patch")) {
        fs::path p = m_var_map["batch-patch"].as<fs::path>();

        if(!fs::is_directory(p))
        {
            BOOST_THROW_EXCEPTION(Exception() << err::file_missing(p.string()));
        }

        m_batch_patch = p;
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
        const creteTraceTag_ty& tt_1 = t1.get_traceTag_explored_nodes();
        const creteTraceTag_ty& tt_2 = t2.get_traceTag_explored_nodes();

        assert(tt_1.size() == tt_2.size());

        for(uint64_t i = 0; i < tt_1.size(); ++i)
        {
            const CreteTraceTagNode& node_1 = tt_1[i];
            const CreteTraceTagNode& node_2 = tt_2[i];

            if(node_1.m_br_taken != node_2.m_br_taken)
            {
                fprintf(stderr, "node[%lu] (%lu): m_br_taken differs:\n",
                        i, tt_1.size());

                fprintf(stderr, "node_1: ");
                print_br_taken(node_1.m_br_taken);
                fprintf(stderr, "\n");
                fprintf(stderr, "node_2: ");
                print_br_taken(node_2.m_br_taken);
                fprintf(stderr, "\n");
            }

            if(node_1.m_last_opc != node_2.m_last_opc)
            {
                fprintf(stderr, "node[%lu] (%lu): m_last_opc differs: %d <=> %d\n",
                        i, tt_1.size(), node_1.m_last_opc, node_2.m_last_opc);
            }

            if(node_1.m_tb_count != node_2.m_tb_count)
            {
                fprintf(stderr, "node[%lu] (%lu): m_tb_count differs: %lu <=> %lu\n",
                        i, tt_1.size(), node_1.m_tb_count, node_2.m_tb_count);
            }

            if(node_1.m_tb_pc != node_2.m_tb_pc)
            {
                fprintf(stderr, "node[%lu] (%lu): m_tb_pc differs: %lu <=> %lu\n",
                        i, tt_1.size(), node_1.m_tb_pc, node_2.m_tb_pc);
            }

        }

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

void apply_tc_patch(vector<TestCase>& tcs)
{
    TestCase base = retrieve_test_serialized("concrete_inputs.bin");

    vector<TestCase> ret;
    ret.reserve(tcs.size());
    for(vector<TestCase>::const_iterator it = tcs.begin();
            it != tcs.end(); ++it) {
        ret.push_back(generate_complete_tc_from_patch(*it, base));
    }

    tcs = ret;
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

    apply_tc_patch(ref_tcs);
    apply_tc_patch(tgt_tcs);

    uint64_t compare_size = ref_tcs.size() < tgt_tcs.size()?ref_tcs.size() : tgt_tcs.size();

    for(uint64_t i = 0; i < compare_size; ++i)
    {
        fprintf(stderr, "\n=================================================\n"
                "compare tc[%lu]\n", i);
        if(!compare_tc_internal(ref_tcs[i], tgt_tcs[i]))
        {
            fprintf(stderr, "-----------------------\n"
                    "ref_tcs: \n");
            ref_tcs[i].print();

            fprintf(stderr, "\n-----------------------\n"
                    "tgt_tcs: \n");
            tgt_tcs[i].print();
        }
        fprintf(stderr, "=================================================\n");
    }
}

void CreteTcCompare::generate_complete_test_from_patch()
{
    vector<TestCase> tcs_patch;;

    if(m_tc_folder)
    {
        tcs_patch = retrieve_tests_serialized(m_patch.string());
    } else {
        tcs_patch.push_back(retrieve_test_serialized(m_patch.string()));
    }

    fs::path out_folder = m_cwd / "compare-tc-patch-out";
    fs::create_directories(out_folder);

    uint64_t out_count = 1;
    TestCase base = retrieve_test_serialized("concrete_inputs.bin");

    for(vector<TestCase>::const_iterator it = tcs_patch.begin();
            it != tcs_patch.end(); ++it)
    {
        TestCase complete = generate_complete_tc_from_patch(*it, base);
        std::stringstream ss;
        ss << out_folder.string() << "/" << out_count++ << ".bin";
        std::ofstream ktest_pool_file(ss.str().c_str(), std::ios_base::out | std::ios_base::binary);
        assert(ktest_pool_file);
        write_serialized(ktest_pool_file, complete);
    }
}

static bool hasEnding (std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

void batch_path_mode_internal(const fs::path& input)
{
    fprintf(stderr, "input = %s\n", input.string().c_str());

    const fs::path tc_dir  = input/"test-case";
    const fs::path base_tc_dir  = input/"test-case-base-cache";

    assert(fs::is_directory(tc_dir));
    assert(fs::is_directory(base_tc_dir));

    vector<TestCase> tcs = retrieve_tests_serialized(tc_dir.string());

    boost::unordered_map<TestCaseHashComplete, TestCase> base_tcs;
    for(fs::directory_iterator it(base_tc_dir); it != fs::directory_iterator(); ++it)
    {
        assert(fs::is_regular(*it));
//        fprintf(stderr, "%s\n", it->path().filename().string().c_str());
        TestCase tc = retrieve_test_serialized(it->path().string());
        TestCaseHashComplete tc_hash = tc.complete_hash();
        base_tcs[tc_hash] = tc;
    }

    const fs::path out_dir  = input/"test-case-parsed";
    if(fs::exists(out_dir))
    {
        fs::remove_all(out_dir);
    }
    fs::create_directories(out_dir);

    for(uint64_t i = 0; i < tcs.size(); ++i)
    {
        TestCase out_tc;
        if(tcs[i].is_test_patch())
        {
            boost::unordered_map<TestCaseHashComplete, TestCase>::const_iterator base_tc =
                    base_tcs.find(tcs[i].get_base_tc_hash());
            assert(base_tc != base_tcs.end());
            out_tc = generate_complete_tc_from_patch(tcs[i], base_tc->second);
        } else {
            out_tc = tcs[i];
        }

        std::stringstream ss;
        ss << out_dir.string() << "/" << (i+1) << ".bin";
        std::ofstream ktest_pool_file(ss.str().c_str(), std::ios_base::out | std::ios_base::binary);
        assert(ktest_pool_file);
        out_tc.write(ktest_pool_file);
    }
}

void CreteTcCompare::batch_path_mode()
{
    for(fs::directory_iterator it(m_batch_patch);
            it != fs::directory_iterator(); ++it) {
        if(!fs::is_directory(*it))
            continue;

        if(!hasEnding(it->path().filename().string(), ".xml"))
            continue;

        batch_path_mode_internal(*it);
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
