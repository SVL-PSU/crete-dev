#ifndef CRETE_DEBUG_H
#define CRETE_DEBUG_H

#include <string>
#include <vector>
#include <set>

#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/filesystem/path.hpp>

namespace crete
{
class Debug
{
public:
    Debug(int argc, char* argv[]);

protected:
    void parse_options(int argc, char* argv[]);
    boost::program_options::options_description make_options();
    void process_options();
    void process_trace_dir(const boost::filesystem::path& dir, std::ostream& os);

    bool is_trace_missing_klee_run(const boost::filesystem::path& trace);
    bool has_linking_failed(const boost::filesystem::path& trace);
    bool has_trace_failed_concrete(const boost::filesystem::path& trace);
    bool has_trace_failed_symbolic(const boost::filesystem::path& trace);
    bool has_trace_failed_tc_gen(const boost::filesystem::path& trace);
    bool has_exception(const boost::filesystem::path& trace);
    std::string get_last_line(boost::filesystem::path& file);
    bool is_last_line_correct(boost::filesystem::path& file);

private:
    boost::program_options::options_description ops_descr_;
    boost::program_options::variables_map var_map_;
};
}

#endif // CRETE_DEBUG_H
