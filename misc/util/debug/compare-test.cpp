#include "compare-test.h"

#include <crete/test_case.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>

#include <fstream>

using namespace std;
using namespace boost;
namespace po = program_options;


namespace crete
{
CompareTest::CompareTest(std::istream& is, std::ostream& os) :
    is_(is),
    os_(os),
    ops_descr_(make_options())
{
}

boost::program_options::options_description CompareTest::make_options()
{
    po::options_description desc("Options");

    desc.add_options()
        ("tc-dir-1,1", po::value<fs::path>(), "Test case folder one")
        ("tc-dir-2,2", po::value<fs::path>(), "Test case folder two")
        ("help", "displays help message")
        ;

    return desc;
}

void CompareTest::start()
{
    display_welcome();
    run_shell();
}

void CompareTest::display_welcome()
{
    std::cout << "crete-debug: compare test initiated" << "\n"
            << "\t--help for the list of options" << "\n"
            << "\t--quit to exit" << endl;
}

void CompareTest::run_shell()
{
    auto next_cmd = string{};

    do
    {
        std::cout << ">> ";
        getline(is_, next_cmd);

    } while(process_options(next_cmd));
}

std::vector<string> CompareTest::tokenize_options(const string& opts)
{
    escaped_list_separator<char> separator("\\", "= ", "\"\'");

    tokenizer<escaped_list_separator<char>> tokens(opts, separator);

    vector<string> args;
    copy_if(tokens.begin(),
            tokens.end(),
            back_inserter(args),
            [](const string& s) { return !s.empty(); });

    return args;
}

bool CompareTest::process_options(const std::string& opts)
{
    auto args = tokenize_options(opts);
    boost::program_options::variables_map var_map;
    try
    {
        po::store(po::command_line_parser(args).options(ops_descr_).run(), var_map);
        po::notify(var_map);
    }
    catch(std::exception& e) // Failed to parse.
    {
        cerr << e.what() << endl;
        return true;
    }

    if(var_map.size() == 0)
    {
        cout << "Unknown command or missing arguments" << endl;
        cout << "Use '--help' for more details" << endl;

        return true;
    }

    if(var_map.count("tc-dir-1"))
    {
        fs::path tc_dir_1;
        fs::path tc_dir_2;

        tc_dir_1 = var_map["tc-dir-1"].as<fs::path>();

        if(var_map.count("tc-dir-2"))
        {
            tc_dir_2 = var_map["tc-dir-2"].as<fs::path>();
        } else {
            cout << "Missing tc-dir-2" << endl;
            return true;
        }

        compare_test(tc_dir_1, tc_dir_2);

        return false;
    }


    if(var_map.count("exit"))
    {
        return false;
    }

    if(var_map.count("help"))
    {
        cout << ops_descr_ << endl;

        return true;
    }

    return false;
}

static string to_test_hash(const TestCase& tc)
{
    std::stringstream ss;
    tc.write(ss);

    return ss.str();
}

static void print_and_reset_matched_range( pair<uint64_t, uint64_t>& matched_range_tc_1,
        pair<uint64_t, uint64_t>& matched_range_tc_2)
{
    if(matched_range_tc_1.first != 0) {
        if(matched_range_tc_1.first == matched_range_tc_1.second)
        {
            fprintf(stderr, "(%lu) : (%lu)\n",
                    matched_range_tc_1.first,
                    matched_range_tc_2.first);
        } else {
            fprintf(stderr, "(%lu - %lu) : (%lu - %lu)\n",
                    matched_range_tc_1.first, matched_range_tc_1.second,
                    matched_range_tc_2.first, matched_range_tc_2.second);
        }

        matched_range_tc_1.first = 0;
        matched_range_tc_2.first = 0;

        matched_range_tc_1.second = 0;
        matched_range_tc_2.second = 0;
    }
}

void CompareTest::compare_test(fs::path tc_dir_1, fs::path tc_dir_2)
{
    if(!fs::exists(tc_dir_1))
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Input test case directory not found: "
                + tc_dir_1.generic_string()));
    }
    if(!fs::exists(tc_dir_2))
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Input test case directory not found: "
                + tc_dir_2.generic_string()));
    }

    vector<TestCase> tcs_1 = retrieve_tests(tc_dir_1.string());
    vector<TestCase> tcs_2 = retrieve_tests(tc_dir_2.string());

    map<uint64_t, string> tcs_map_1;
    for(uint64_t i = 1; i <= tcs_1.size(); ++i)
    {
        tcs_map_1[i] = to_test_hash(tcs_1[i-1]);
    }

    map<string, uint64_t> tcs_map_2;
    for(uint64_t i = 1; i <= tcs_2.size(); ++i)
    {
        tcs_map_2[to_test_hash(tcs_2[i-1])] = i;
    }

    // <tc_index_1, tc_index_2>
    map<uint64_t, uint64_t> tcs_map;
    for(map<uint64_t, string>::const_iterator it = tcs_map_1.begin();
            it != tcs_map_1.end(); ++it) {
        map<string, uint64_t>::const_iterator temp_it = tcs_map_2.find(it->second);
        if(temp_it == tcs_map_2.end())
            continue;

        tcs_map.insert(make_pair(it->first, temp_it->second));
    }

    fprintf(stderr, "tcs_1 = %lu, tcs_2 = %lu\n"
            "tcs_map_1 = %lu, tcs_map_2 = %lu\n",
            tcs_1.size(), tcs_2.size(),
            tcs_map_1.size(), tcs_map_2.size());

    // Print out matched test cases
    pair<uint64_t, uint64_t> matched_range_tc_1;
    pair<uint64_t, uint64_t> matched_range_tc_2;

    bool print_range = false;
    for(uint64_t i = 1; i <= tcs_map_1.size(); ++i)
    {
//        cerr << "iteration: " << i << endl;
        map<uint64_t, uint64_t>::const_iterator temp_it = tcs_map.find(i);
        if(temp_it == tcs_map.end())
        {
            print_and_reset_matched_range(matched_range_tc_1, matched_range_tc_2);
            fprintf(stderr, "(%lu) : (NONE)\n", i);
        } else {
            if(matched_range_tc_1.first == 0){
                matched_range_tc_1.first = temp_it->first;
                matched_range_tc_2.first = temp_it->second;

                matched_range_tc_1.second = temp_it->first;
                matched_range_tc_2.second = temp_it->second;
            } else {
                if(matched_range_tc_1.second == (temp_it->first - 1) &&
                        matched_range_tc_2.second == (temp_it->second -1)) {

                    matched_range_tc_1.second= temp_it->first;
                    matched_range_tc_2.second = temp_it->second;
                } else {
                    print_and_reset_matched_range(matched_range_tc_1, matched_range_tc_2);

                    fprintf(stderr, "(%lu) : (%lu)\n",
                            temp_it->first, temp_it->second);
                }
            }
        }
    }

    print_and_reset_matched_range(matched_range_tc_1, matched_range_tc_2);
}

}



