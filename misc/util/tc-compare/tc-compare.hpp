#ifndef CRETE_TC_COMPARE_H
#define CRETE_TC_COMPARE_H

#include <crete/test_case.h>
#include <crete/harness_config.h>

#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>

#include <string>
#include <vector>

using namespace std;
namespace crete
{
namespace fs = boost::filesystem;
namespace po = boost::program_options;

class CreteTcCompare
{
private:
    po::options_description m_ops_descr;
    po::variables_map m_var_map;

    fs::path m_cwd;
    fs::path m_ref;
    fs::path m_tgt;

    fs::path m_patch;
    fs::path m_batch_patch;

    bool m_tc_folder;

public:
    CreteTcCompare(int argc, char* argv[]);

private:
    boost::program_options::options_description make_options();
    void process_options(int argc, char* argv[]);

    void compare_tc();
    void generate_complete_test_from_patch();

    void batch_path_mode();
};

}

#endif // CRETE_TC_COMPARE_H
