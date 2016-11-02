#include "coverage.h"

#include <cstdlib>
#include <sstream>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;

namespace crete
{

const char* CRETE_COVERAGE_DIR_PATH = "coverage_report";
const char* CRETE_COVERAGE_TEST_CASE_NAME = "test_case.bin";

void write(const TestCase& tc, const boost::filesystem::path& p)
{
    boost::filesystem::ofstream ofs(p, ios_base::out | ios_base::binary);

    tc.write(ofs);
}

Coverage::Coverage(const boost::property_tree::ptree& properties) :
    properties_(properties)
{
    boost::optional<string> sopt = properties_.get_optional<string>("target.binary");
    if(sopt)
        target_binary_ = *sopt;
}

void Coverage::generate(const TestCase& tc)
{
    namespace fs = boost::filesystem;

    fs::path coverage_path(CRETE_COVERAGE_DIR_PATH);
    if(!fs::exists(coverage_path))
        throw runtime_error("unable to find coverage directory: " + coverage_path.generic_string());

    write(tc, coverage_path / CRETE_COVERAGE_TEST_CASE_NAME);

    stringstream exe_binary;
    exe_binary << "("
               << "cd "
               << coverage_path.generic_string()
               << " && ./"
               << target_binary_
               << " &> /dev/null"
               << ")";
    system(exe_binary.str().c_str());

    string lcov = "lcov -t 'test' -o " +
            coverage_path.generic_string() +
            "/bin_coverage.info -c -d " +
            coverage_path.generic_string() + " &> /dev/null";
    system(lcov.c_str());

    string genhtml = "genhtml -o " +
            coverage_path.generic_string() +
            "/result " +
            coverage_path.generic_string() +
            "/bin_coverage.info | grep \"coverage rate\" -A3";
    system(genhtml.c_str());
}

void Coverage::clean()
{
    namespace fs = boost::filesystem;

    fs::path coverage_path(CRETE_COVERAGE_DIR_PATH);
    if(!fs::exists(coverage_path))
        return;

    stringstream ss_gcda;
    ss_gcda << "("
            << "cd "
            << coverage_path.generic_string()
            << " && rm "
            << target_binary_
            << ".gcda &> /dev/null"
            << ")";
    system(ss_gcda.str().c_str());

    stringstream ss_info;
    ss_info << "("
            << "cd "
            << coverage_path.generic_string()
            << " && rm -r bin_coverage.info result &> /dev/null "
            << ")";
    system(ss_info.str().c_str());
}

}
