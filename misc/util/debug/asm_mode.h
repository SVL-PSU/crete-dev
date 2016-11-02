#ifndef CRETE_ASM_MODE_H
#define CRETE_ASM_MODE_H

#include <string>
#include <vector>
#include <set>

#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/filesystem/path.hpp>

namespace crete
{

struct AddressRange
{
    boost::filesystem::path path_;
    uint64_t low_ = 0;
    uint64_t high_ = 0;
};

bool operator==(const AddressRange& lhs, const AddressRange& rhs);
bool operator==(const AddressRange& lhs, const boost::filesystem::path& rhs);
bool operator==(const boost::filesystem::path& rhs, const AddressRange& lhs);
bool operator==(const AddressRange& lhs, const uint64_t& rhs);
bool operator==(const uint64_t& rhs, const AddressRange& lhs);

class ASMMode
{
public:
    ASMMode(std::istream& is, std::ostream& os);

    void start();

protected:
    void display_welcome();
    void run_shell();
    boost::program_options::options_description make_options();
    bool process_options(const std::string& opts);
    std::vector<std::string> tokenize_options(const std::string& opts);
    void parse_proc_maps(const boost::filesystem::path& path);
    bool have_proc_map() const;
    void process_address(uint64_t addr) const;
    uint64_t parse_hex(const std::string& s) const;
    uint64_t parse_dec(const std::string& s) const;

private:
    std::istream& is_;
    std::ostream& os_;
    boost::program_options::options_description ops_descr_;
    std::vector<AddressRange> addr_ranges_;
};

}

#endif // CRETE_ASM_MODE_H
