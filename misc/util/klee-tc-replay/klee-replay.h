#ifndef CRETE_REPLAY_H
#define CRETE_REPLAY_H

#include <boost/process.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>

#include <string>
#include <vector>

using namespace std;
namespace crete
{
namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace bp = boost::process;

class KleeReplay
{
private:
    po::options_description m_ops_descr;
    po::variables_map m_var_map;

    fs::path m_klee_replay;
    fs::path m_exec;
    fs::path m_tc_dir;

    fs::path m_cwd;
    fs::path m_launch_directory;
    vector<string> m_launch_args;
    bp::posix_context m_launch_ctx;

    fs::path m_input_sandbox;
    fs::path m_environment;
    bool m_init_sandbox;
    bool m_enable_log;

public:
    KleeReplay(int argc, char* argv[]);

private:
    boost::program_options::options_description make_options();
    void process_options(int argc, char* argv[]);
    void setup_launch();

    void init_sandbox();
    void reset_sandbox();
    void reset_sandbox_folder_permission();

    void collect_gcov_result();
    void replay();
};

}

#endif // CRETE_REPLAY_H
