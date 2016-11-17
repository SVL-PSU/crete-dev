#ifndef MISC_UTIL_DEBUG_COMPARE_TEST_H_
#define MISC_UTIL_DEBUG_COMPARE_TEST_H_

#include <string>
#include <vector>
#include <set>

#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/filesystem/path.hpp>

namespace fs = boost::filesystem;

namespace crete
{

class CompareTest
{
private:
    std::istream& is_;
    std::ostream& os_;
    boost::program_options::options_description ops_descr_;

public:
    CompareTest(std::istream& is, std::ostream& os);
    void start();

private:
    boost::program_options::options_description make_options();
    void display_welcome();
    void run_shell();
    std::vector<std::string> tokenize_options(const std::string& opts);
    bool process_options(const std::string& opts);

    void compare_test(fs::path tc_dir_1, fs::path tc_dir_2);
};

}



#endif /* MISC_UTIL_DEBUG_COMPARE_TEST_H_ */
