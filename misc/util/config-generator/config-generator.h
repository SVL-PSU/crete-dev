#ifndef CRETE_CONFIG_GENERATOR_H
#define CRETE_CONFIG_GENERATOR_H
#include <string>
#include <vector>
#include <stdint.h>
#include <map>

#include <crete/run_config.h>

using namespace std;
using namespace crete;

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
    void gen_config();
    void consistency_check();
    boost::filesystem::path addConfigOutputDir(string name);
    void gen_crete_test_internal();
    void gen_crete_test_seeds(boost::filesystem::path seeds_folder);

private:
    config::RunConfiguration m_crete_config;
    string m_target_exec;
    set<string> m_seeds;
    string m_outputDirectory;
};

class CreteTests
{
public:
    CreteTests(const char *input_file);
    ~CreteTests() {}

    void gen_crete_tests(bool inject_one_concolic);
    void gen_crete_tests_coreutils();

private:
    void parse_cmdline_tests(const char *input_file);
    void initBaseOutputDirectory();
private:
    // <test pattern string, crete test>
//    multimap<string, string> m_crete_tests;
    map<string, CreteTest> m_crete_tests;
    string m_outputDirectory;
};
#endif // #ifndef CRETE_CONFIG_GENERATOR_H
