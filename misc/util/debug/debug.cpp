#include "debug.h"
#include "asm_mode.h"
#include "compare-test.h"

#include <boost/program_options.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

#include <iostream>

using namespace std;
using namespace boost;
namespace fs = filesystem;
namespace po = program_options;

namespace crete
{

Debug::Debug(int argc, char* argv[]) :
    ops_descr_(make_options())
{
    parse_options(argc, argv);
    process_options();
}

po::options_description Debug::make_options()
{
    po::options_description desc("Options");

    desc.add_options()
        ("help,h", "displays help message")
        ("trace-dir,t", po::value<fs::path>(), "trace directory")
        ("asm-mode,a", "assembly mode")
        ("out-file,o", po::value<fs::path>(), "output file")
        ("exception,e", "search for CRETE exceptions")
        ("compare-test,c", "compare the test cases between two folder")
            ;

    return desc;
}

void Debug::process_options()
{
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
    if(var_map_.count("asm-mode"))
    {
        if(var_map_.count("out-file"))
        {
            fs::ofstream ofs(var_map_["out-file"].as<fs::path>());
            if(!ofs)
            {
                cout << "can't open output file: " << var_map_["out-file"].as<fs::path>();
                exit(1);
            }
            ASMMode amode(cin, ofs);

            amode.start();
        }
        else
        {
            ASMMode amode(cin, cout);

            amode.start();
        }

        exit(0);
    }

    if(var_map_.count("compare-test"))
    {
        CompareTest compare_test(cin, cout);

        compare_test.start();

        exit(0);
    }

    if(var_map_.count("trace-dir"))
    {
        auto trace_dir = var_map_["trace-dir"].as<fs::path>();
        if(!fs::exists(trace_dir))
        {
            cout << "invalid path: " << trace_dir << endl;
            exit(1);
        }

        if(var_map_.count("out-file"))
        {
            fs::ofstream ofs(var_map_["out-file"].as<fs::path>());
            if(!ofs)
            {
                cout << "can't open output file: " << var_map_["out-file"].as<fs::path>();
                exit(1);
            }
            process_trace_dir(fs::absolute(trace_dir), ofs);
        }
        else
        {
            process_trace_dir(fs::absolute(trace_dir), cout);
        }
    }
}

void Debug::process_trace_dir(const fs::path& dir, ostream& os)
{
    os << "Processing: " << dir << endl;

    set<fs::path> missing_klee_run;
    set<fs::path> failed_linking;
    set<fs::path> failed_concrete;
    set<fs::path> failed_symbolic;
    set<fs::path> failed_test_generation;
    set<fs::path> exception_generated;
    size_t traces_processed = 0;

    fs::directory_iterator db(dir), de;
    BOOST_FOREACH(const fs::path& trace, make_pair(db, de))
    {
        if(fs::is_directory(trace))
        {
            if(regex_match(trace.filename().string(),
                           regex("[a-z0-9]+(-)[a-z0-9]+(-)[a-z0-9]+(-)[a-z0-9]+(-)[a-z0-9]+")))
            {
                if(is_trace_missing_klee_run(trace))
                    missing_klee_run.insert(trace);
                else if(has_linking_failed(trace))
                    failed_linking.insert(trace);
                else if(has_trace_failed_concrete(trace))
                    failed_concrete.insert(trace);
                else if(has_trace_failed_symbolic(trace))
                    failed_symbolic.insert(trace);
                else if(has_trace_failed_tc_gen(trace))
                    failed_test_generation.insert(trace);

                if(var_map_.count("exception"))
                {
                    if(has_exception(trace))
                        exception_generated.insert(trace);
                }

                ++traces_processed;
            }
        }
    }

    if(failed_linking.size())
    {
        os << "Linking failed: " << failed_linking.size() << endl;
        for(const auto& trace : failed_linking)
        {
            os << "\t" << trace.stem() << "\n";
        }
    }
    if(failed_concrete.size())
    {
        os << "Concrete failed: " << failed_concrete.size() << endl;
        for(const auto& trace : failed_concrete)
        {
            os << "\t" << trace.stem() << "\n";
        }
    }
    if(failed_symbolic.size())
    {
        os << "Symbolic failed: " << failed_symbolic.size() << endl;
        for(const auto& trace : failed_symbolic)
        {
            os << "\t" << trace.stem() << "\n";
        }
    }
    if(failed_test_generation.size())
    {
        os << "Test generation failed: " << failed_test_generation.size() << endl;
        for(const auto& trace : failed_test_generation)
        {
            os << "\t" << trace.stem() << "\n";
        }
    }
    if(exception_generated.size())
    {
        os << "Exception generated: " << exception_generated.size() << endl;
        for(const auto& trace : exception_generated)
        {
            os << "\t" << trace.stem() << "\n";

            assert(fs::is_directory(trace));
            auto except_dir = trace/"klee-run"/"exception";
            assert(fs::is_directory(except_dir));

            fs::directory_iterator tb(except_dir), te;
            BOOST_FOREACH(const fs::path& except, make_pair(tb, te))
            {
                fs::directory_iterator eb(except), ee;
                auto count = count_if(eb, ee, [](const fs::directory_entry& d) {
                    return !fs::is_directory(d);
                });
                os << "\t\t" << except.stem()  << ": " << count << "\n";
            }
        }
    }

    os << "Traces executed: " << traces_processed - missing_klee_run.size() << "\n";
    os << "Traces processed: " << traces_processed << "\n";
    if(failed_linking.size() == 0 &&
       failed_concrete.size() == 0 &&
       failed_symbolic.size() == 0 &&
       failed_test_generation.size() == 0
       ) os << "***Status: Hunky-Dory***" << endl;
    else
        os << "***Status: Problems Found***" << endl;
}

bool Debug::is_trace_missing_klee_run(const filesystem::path& trace)
{
    return !fs::exists(trace/"klee-run");
}

bool Debug::has_linking_failed(const filesystem::path& trace)
{
    return !fs::exists(trace/"klee-run"/"run.bc");
}

bool Debug::has_trace_failed_concrete(const filesystem::path& trace)
{
    auto concolic = trace/"klee-run"/"concolic.log";
    if(!exists(concolic))
        return true;

    return !is_last_line_correct(concolic);
}

bool Debug::has_trace_failed_symbolic(const filesystem::path& trace)
{
    auto klee_run = trace/"klee-run"/"klee-run.log";
    if(!exists(klee_run))
        return true;

    return !is_last_line_correct(klee_run);
}

bool Debug::has_trace_failed_tc_gen(const filesystem::path& trace)
{
    auto ktest_pool = trace/"klee-run"/"ktest_pool";

    if(!fs::exists(ktest_pool))
        return true;

    fs::directory_iterator db(ktest_pool), de;
    BOOST_FOREACH(const fs::path& trace, make_pair(db, de))
    {
        if(fs::is_regular_file(trace))
        {
            return false;
        }
    }

    return true;
}

bool Debug::has_exception(const filesystem::path& trace)
{
    auto except = trace/"klee-run"/"exception";

    return fs::exists(except);
}

string Debug::get_last_line(fs::path& file)
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

bool Debug::is_last_line_correct(filesystem::path& file)
{
    auto line = get_last_line(file);

    return regex_match(line, regex("(KLEE: done: generated tests =)(.)*"));
}

void Debug::parse_options(int argc, char* argv[])
{
    po::store(po::parse_command_line(argc, argv, ops_descr_), var_map_);
    po::notify(var_map_);
}

} // namespace crete

int main(int argc, char* argv[])
{
    try
    {
        vector<string> args;

        crete::Debug debug(argc, argv);
    }
    catch(std::exception& e)
    {
        cerr << "Error: " << e.what() << endl;
        return -1;
    }

    return 0;
}
