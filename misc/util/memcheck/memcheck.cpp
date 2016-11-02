#include "memcheck.h"

#include <iomanip>

#include <boost/program_options.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <boost/assign/list_of.hpp>

#include <crete/test_case.h>

#include <boost/process.hpp>

using namespace std;
using namespace boost;
namespace fs = filesystem;
namespace po = program_options;
namespace bp = process;

namespace crete
{

MemoryChecker::MemoryChecker(int argc, char* argv[]) :
    ops_descr_(make_options())
{
    parse_options(argc, argv);
    process_options();
}

void MemoryChecker::parse_options(int argc, char* argv[])
{
    po::store(po::parse_command_line(argc, argv, ops_descr_), var_map_);
    po::notify(var_map_);
}

po::options_description MemoryChecker::make_options()
{
    po::options_description desc("Options");

    desc.add_options()
        ("help,h", "displays help message")
        ("config,c", po::value<fs::path>(), "configuration file (found in guest-data/)")
        ("exec,e", po::value<fs::path>(), "executable to test")
        ("tc-dir,t", po::value<fs::path>(), "test case directory")
        ("stack-check,s", "checks only for stack-related errors")
        ("mem-check,m", "checks for non-stack related errors")
        ;

    return desc;
}

void MemoryChecker::process_options()
{
    fs::path config_path;

    if(var_map_.size() == 0)
    {
        cout << "Missing arguments" << endl;
        cout << "Use '--help' for more details" << endl;
        exit(0);
    }
    if(var_map_.count("help"))
    {
        cout << ops_descr_ << endl;
        exit(0);
    }
    if(var_map_.count("exec"))
    {
        exec_ = var_map_["exec"].as<fs::path>();
        if(!fs::exists(exec_))
            throw std::runtime_error("[exec] executable not found: " + exec_.generic_string());
    }
    else
    {
        throw std::runtime_error("required: option [exec]. See '--help' for more info");
    }
    if(var_map_.count("tc-dir"))
    {
        tc_dir_ = var_map_["tc-dir"].as<fs::path>();
        if(!fs::exists(tc_dir_))
            throw std::runtime_error("[tc-dir] input directory not found: " + tc_dir_.generic_string());
    }
    else
    {
        throw std::runtime_error("required: option [tc-dir]. See '--help' for more info");
    }
    if(var_map_.count("config"))
    {
        config_path = var_map_["config"].as<fs::path>();
        if(!fs::exists(config_path))
        {
            throw std::runtime_error("[config] file not found: " + config_path.string());
        }

        fs::path out_path = "harness.config.serialized";

        if(!fs::equivalent(config_path, out_path))
        {
            fs::ifstream ifs(config_path);
            if(!ifs.good())
            {
                throw std::runtime_error("[config] unable to open file: " + config_path.string());
            }

            fs::ofstream ofs(out_path);
            if(!ofs.good())
            {
                throw std::runtime_error("[config] unable to open file: " + out_path.string());
            }

            ofs << ifs.rdbuf();
        }
    }
    else
    {
        throw std::runtime_error("required: option [config]. See '--help' for more info");
    }

    auto checked = false;

    boost::filesystem::path cwd( boost::filesystem::current_path() );
    if(var_map_.count("stack-check"))
    {
        MemoryCheckerExecutor executor(MemoryCheckerExecutor::Type::stack,
                                       exec_,
                                       tc_dir_,
                                       cwd,
                                       config_path);

        executor.execute_all();
        executor.print_summary(cout);

        checked = true;
    }
    if(var_map_.count("mem-check"))
    {

        MemoryCheckerExecutor executor(MemoryCheckerExecutor::Type::memory,
                                       exec_,
                                       tc_dir_,
                                       cwd,
                                       config_path);

        executor.execute_all();
        executor.print_summary(cout);

        checked = true;
    }

    if(!checked)
    {
        throw std::runtime_error("required: option [stack-check] and/or [mem-check]. See '--help for more info");
    }
}

MemoryCheckerExecutor::MemoryCheckerExecutor(MemoryCheckerExecutor::Type type,
                                             const filesystem::path& executable,
                                             const filesystem::path& test_case_dir,
                                             const boost::filesystem::path& working_dir,
                                             const boost::filesystem::path& configuration) :
    Executor(executable, test_case_dir, working_dir, configuration),
    type_(type),
    log_file_("crete.output.txt"), // Re-use Executor's
    error_count_(0)
{
    fs::remove_all(working_directory() / "defect");
}

void MemoryCheckerExecutor::print_summary(ostream& os)
{
    os << "Summary:\n"
       << "\tDefect inducing test cases: "
       << error_count_
       << "\n";
    if(error_count_ > 0)
    {
        os << "Reports: \n"
           << "\t"
           << (working_directory() / "defect").generic_string()
           << "\n";
    }
}

void MemoryCheckerExecutor::clean()
{
    Executor::clean();

    fs::remove(working_directory() / log_file_);
}

void MemoryCheckerExecutor::execute()
{
    auto valgrind = bp::find_executable_in_path("valgrind");

    std::vector<std::string> args = boost::assign::list_of("valgrind");

    if(type_ == Type::stack)
    {
        args.push_back("--tool=exp-sgcheck");
    }
    else if(type_ == Type::memory)
    {
        args.push_back("--tool=memcheck");
        args.push_back("--leak-check=full");
    }

    args.push_back(binary().string());

    Executor::execute(valgrind, args);

    process_log(log_file_);
}

void MemoryCheckerExecutor::print_status_header(ostream& os)
{
    os << std::setw(14) << "defects"
       << "|";

    Executor::print_status_header(os);
}

void MemoryCheckerExecutor::print_status_details(ostream& os)
{
    os << setw(14) << error_count_
       << "|";

    Executor::print_status_details(os);
}

void MemoryCheckerExecutor::process_log(const filesystem::path& log_path)
{
    assert(fs::exists(log_path)); // TODO: throw instead

    fs::ifstream ifs(log_path);

    assert(ifs.good()); // TODO: throw instead

    auto line = get_last_line(log_path);

    boost::smatch m;
    boost::regex e ("(ERROR SUMMARY: )([[:digit:]]+)");

    boost::regex_search(line, m, e);

    assert(m.size() == 3);

    auto errors = std::stoul(*(m.begin() + 2));

    if(errors > 0)
    {
        ++error_count_;

        auto working = working_directory();
        auto defect_dir = working / "defect";

        if(!fs::exists(defect_dir))
        {
            fs::create_directory(defect_dir);
        }

        auto current_tc = current_test_case();
        auto tc_filename = current_tc.filename();
        // Boost 1.55 copy_file won't work with -std=c++11 (bug), so have to use alternative.
        // TODO: replace with fs::copy_file after upgrading Boost.
        fs::ifstream ifs_tc(current_tc);
        fs::ofstream ofs_tc(defect_dir / tc_filename);
        ofs_tc << ifs_tc.rdbuf();

        fs::rename(log_path,
                   defect_dir / (tc_filename.generic_string() + ".report." + to_string(type_) + ".txt"));
    }
}

string MemoryCheckerExecutor::get_last_line(const filesystem::path& file)
{
    fs::ifstream ifs(file);
    if(!ifs)
        throw runtime_error("failed open file for examination: " + file.generic_string());

    ifs.seekg(-2, ios::end);
    char c;
    do
    {
        if(ifs.tellg() < 2)
            break;

        ifs.get(c);
        ifs.seekg(-2, ios::cur);
    }while(c != '\n');

    ifs.seekg(2, ios::cur);

    string line;
    getline(ifs, line);

    return line;
}

string MemoryCheckerExecutor::to_string(MemoryCheckerExecutor::Type type)
{
    switch(type)
    {
    case Type::stack:
        return "stack";
    case Type::memory:
        return "memory";
    default:
        assert(0);
    }
}

} // namespace crete

int main(int argc, char* argv[])
{
    try
    {
        crete::MemoryChecker memchecker(argc, argv);
    }
    catch(std::exception& e)
    {
        cerr << "[CRETE] Exception: " << e.what() << endl;
        return -1;
    }

    return 0;
}
