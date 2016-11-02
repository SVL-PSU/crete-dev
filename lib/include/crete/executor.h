#ifndef CRETE_TEST_CASE_EXECUTOR_H
#define CRETE_TEST_CASE_EXECUTOR_H

#include <crete/dll.h>
#include <crete/test_case.h>
#include <crete/harness_config.h>

#include <boost/filesystem.hpp>

namespace crete
{

class CRETE_DLL_EXPORT Executor
{
public:
    enum ElemType { elem_arg, elem_file, elem_stdin };
    typedef std::deque<ElemType> ElemTypeQueue;

public:
    Executor(const boost::filesystem::path& binary,
             const boost::filesystem::path& test_case_dir,
             const boost::filesystem::path& working_dir,
             const boost::filesystem::path& configuration);
    virtual ~Executor();

    void execute_all();
    void execute_seed_only();

    const boost::filesystem::path& binary() const { return bin_; }
    const boost::filesystem::path& test_case_directory() const { return test_case_dir_; }
    const boost::filesystem::path& working_directory() const { return working_dir_; }
    const boost::filesystem::path& current_test_case() const { return current_tc_; }

protected:
    virtual void clean();
    virtual void prepare();
    virtual void execute();
    virtual void execute(const std::string& exe, const std::vector<std::string>& args);
    virtual void print_status(std::ostream& os);
    virtual void print_status_header(std::ostream& os);
    virtual void print_status_details(std::ostream& os);

    void count_test_cases();
    void load_configuration(const boost::filesystem::path& path);
    void load_element_type_sequence(const config::HarnessConfiguration& hconfig);
    const ElemTypeQueue& element_type_sequence() const;
    void write(const TestCaseElement& tce) const;
    TestCaseElements filter(const TestCaseElements& elems) const;
    config::Arguments sort_by_index(const config::Arguments& args);
    std::string escape(const std::string& s);
    void write_arguments_file(const config::Arguments& args);
    TestCaseElement select_stdin(const TestCaseElement& lhs, const TestCaseElement& rhs) const;
    TestCaseElement select_file(const TestCaseElement& lhs, const TestCaseElement& rhs) const;

private:
    boost::filesystem::path bin_;
    boost::filesystem::path test_case_dir_;
    boost::filesystem::path working_dir_;
    boost::filesystem::path current_tc_;
    std::size_t test_case_count_;
    std::size_t iteration_;
    std::size_t iteration_print_threshold_;
    config::HarnessConfiguration harness_config_;
    ElemTypeQueue elem_type_seq_;
    config::Arguments current_args_;
    std::vector<uint8_t> current_stdin_data_;
};

template <typename Exec>
Exec make_executor(int argc, char* argv[])
{
    namespace fs = boost::filesystem;

    if(argc != 3)
        throw std::runtime_error("arguments: <binary> <test_cases_path>");

    fs::path bin(argv[1]);
    fs::path tcs(argv[2]);

    if(!fs::exists(bin))
        throw std::runtime_error("binary file not found!");
    if(!fs::exists(tcs))
        throw std::runtime_error("test cases path could not be found!");

    return Exec(bin, tcs);
}

}

#endif // CRETE_TEST_CASE_EXECUTOR_H
