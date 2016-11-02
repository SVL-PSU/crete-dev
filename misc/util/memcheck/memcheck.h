#ifndef CRETE_MEMCHECK_H
#define CRETE_MEMCHECK_H

#include <string>
#include <vector>
#include <set>

#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/filesystem/path.hpp>

#include <crete/executor.h>

namespace crete
{
class MemoryCheckerExecutor;

class MemoryChecker
{
public:
    MemoryChecker(int argc, char* argv[]);

protected:
    void parse_options(int argc, char* argv[]);
    boost::program_options::options_description make_options();
    void process_options();

private:
    boost::program_options::options_description ops_descr_;
    boost::program_options::variables_map var_map_;
    boost::filesystem::path exec_;
    boost::filesystem::path tc_dir_;
};

class MemoryCheckerExecutor : public Executor
{
public:
    enum class Type { stack, memory };

public:
    MemoryCheckerExecutor(MemoryCheckerExecutor::Type type,
                          const boost::filesystem::path& executable,
                          const boost::filesystem::path& test_case_dir,
                          const boost::filesystem::path& working_dir,
                          const boost::filesystem::path& configuration);

    void print_summary(std::ostream& os);

protected:
    void clean() override;
    void execute() override;
    void print_status_header(std::ostream& os) override;
    void print_status_details(std::ostream& os) override;

    void process_log(const boost::filesystem::path& log_path);
    std::string get_last_line(const boost::filesystem::path& file);
    std::string to_string(Type type);

private:
    Type type_;
    const std::string log_file_;
    std::size_t error_count_;
};

}

#endif // CRETE_MEMCHECK_H
