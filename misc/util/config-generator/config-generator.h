#ifndef CRETE_CONFIG_GENERATOR_H
#define CRETE_CONFIG_GENERATOR_H
#include <string>
#include <vector>
#include <stdint.h>
#include <map>

#include <crete/run_config.h>

#include <boost/program_options.hpp>

using namespace std;
using namespace crete;

namespace po = boost::program_options;

class CreteTest
{
public:
    CreteTest() {}
    ~CreteTest() {};

    void add_seed(const string& seed);
    uint64_t get_seed_size() const;
    void print_all_seeds() const;

    void gen_crete_test(bool inject_one_concolic);
    void set_outputDir(string outputDirectory);

private:
    uint64_t get_max_seed_arg_size(uint64_t index) const;
    uint64_t get_max_seed_file_size(uint64_t index) const;

    void gen_config();
    void consistency_check() const;
    boost::filesystem::path addConfigOutputDir(string name);
    void gen_crete_test_internal();
    void gen_crete_test_seeds(boost::filesystem::path seeds_folder) const;

private:
    config::RunConfiguration m_crete_config;
    string m_target_exec;

    map<string, uint32_t> m_seeds_map;
    vector<vector<string> > m_seeds_args;
    vector<vector<string> > m_seeds_files;

    string m_outputDirectory;
};

struct ParsedSymArgs;

class CreteTests
{
public:
    CreteTests(const char *input_file);
    ~CreteTests() {}

    void gen_crete_tests(bool inject_one_concolic);
    void gen_crete_tests_coreutils_grep_diff(string suite_name);

private:
    void parse_cmdline_tests(const char *input_file);
    void initBaseOutputDirectory();

    void generate_crete_config(const ParsedSymArgs& parsed_config, const string& exec_name);
private:
    // <test pattern string, crete test>
//    multimap<string, string> m_crete_tests;
    map<string, CreteTest> m_crete_tests;
    string m_outputDirectory;

    uint64_t m_config_count;
};

class CreteConfig
{
private:
    boost::program_options::options_description m_ops_descr;
    boost::program_options::variables_map m_var_map;

public:
    CreteConfig(int argc, char* argv[]);

private:
    po::options_description make_options();
    void parse_options(int argc, char* argv[]);
    void process_options();
};
#endif // #ifndef CRETE_CONFIG_GENERATOR_H
