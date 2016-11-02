#include "asm_mode.h"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>

#include <crete/proc_reader.h>
#include <fstream>

using namespace std;
using namespace boost;
namespace fs = filesystem;
namespace po = program_options;

namespace crete
{

ASMMode::ASMMode(std::istream& is, std::ostream& os) :
    is_(is),
    os_(os),
    ops_descr_(make_options())
{
}

void ASMMode::start()
{
    display_welcome();
    run_shell();
}

void ASMMode::display_welcome()
{
    std::cout << "ASM mode initiated" << "\n"
            << "\t--help for the list of options" << "\n"
            << "\t--quit to exit" << endl;
}

void ASMMode::run_shell()
{
    auto next_cmd = string{};

    do
    {
        std::cout << ">> ";
        getline(is_, next_cmd);

    }while(process_options(next_cmd));
}

boost::program_options::options_description ASMMode::make_options()
{
    po::options_description desc("Options");

    desc.add_options()
        ("abs-addr,a", po::value<string>(), "get absolute address (hex (0xN) or dec)")
        ("batch,b", po::value<fs::path>(), "batch mode of hex mode")
        ("dec-addr,d", po::value<string>(), "get absolute address (dec)")
        ("hex-addr,h", po::value<string>(), "get absolute address (hex)")
        ("quit,q", "exit shell")
        ("help", "displays help message")
        ("proc-maps,p", po::value<fs::path>(), "set proc-maps file")
        ;

    return desc;
}

bool ASMMode::process_options(const std::string& opts)
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
    if(var_map.count("abs-addr"))
    {
        auto addr = var_map["abs-addr"].as<string>();
        uint64_t res = 0;

        if(addr.at(0) == '0' && addr.at(1) == 'x')
        {
            res = parse_hex(addr);
        }
        else
        {
            res = parse_dec(addr);
        }

        process_address(res);

        return true;
    }
    if(var_map.count("hex-addr"))
    {
        auto addr = var_map["hex-addr"].as<string>();

        process_address(parse_hex(addr));

        return true;
    }
    if(var_map.count("dec-addr"))
    {
        auto addr = var_map["dec-addr"].as<string>();

        process_address(parse_dec(addr));

        return true;
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
    if(var_map.count("proc-maps"))
    {
        auto path = var_map["proc-maps"].as<fs::path>();
        if(!fs::exists(path))
        {
            cerr << "invalid path: " << path << endl;
            return true;
        }

        parse_proc_maps(path);

        return true;
    }
    if(var_map.count("batch"))
    {
        auto path = var_map["batch"].as<fs::path>();
        if(!fs::exists(path))
        {
            cerr << "invalid path: " << path << endl;
            return true;
        }

        std::ifstream in_f(path.string(), std::ifstream::in);

        std::string addr;
        while(std::getline(in_f, addr)) {
            if(!addr.empty()){
//                fprintf(stderr, "addr = 0x%s\n", addr.c_str());
                process_address(parse_hex(addr));
            }
        }

        return true;
    }


    return false;
}

std::vector<string> ASMMode::tokenize_options(const string& opts)
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

void ASMMode::parse_proc_maps(const filesystem::path& path)
{
    addr_ranges_.clear();

    ProcReader reader{path};

    auto maps = reader.find_all();

    for(const auto& m : maps)
    {
        auto range = AddressRange{};

        range.low_ = m.address().first;
        range.high_ = m.address().second;
        range.path_ = m.path();

        addr_ranges_.push_back(range);
    }
}

bool ASMMode::have_proc_map() const
{
    return !addr_ranges_.empty();
}

void ASMMode::process_address(uint64_t addr) const
{
    if(have_proc_map())
    {
        auto it = find_if(addr_ranges_.begin(), addr_ranges_.end(), [&addr](const AddressRange& range) {
           if(range.high_ >= addr &&
              range.low_ <= addr) return true;
           return false;
        });

        if(it != addr_ranges_.end())
        {
            auto& r = *it;

            if(it->path_.stem() != "harness") // Don't offset the binary.
            {
                auto offset = addr - r.low_;

                os_ << "0x"
                    << hex << offset << dec
                    << " : "
                    << r.path_.stem()
                    << endl;

                return;
            }
            else
            {
                os_ << "0x"
                    << hex << addr << dec
                    << " : "
                    << "harness"
                    << endl;

                return;
            }
        }
        else
        {
            os_ << "Warning: address not in any proc-maps ranges\n";
        }
    }
    else
    {
        os_ << "proc_map does not exist!\n";
    }

    os_ << "0x"
        << hex << addr << dec
        << endl;
}

uint64_t ASMMode::parse_hex(const string& s) const
{
    uint64_t res = 0;

    stringstream ss;
    if(s.at(0) == '0' && s.at(1) == 'x')
    {
        ss << hex << s.substr(2, s.size() - 2);
        ss >> hex >> res;
    }
    else // Still hex, but no 0x-prefix.
    {
        ss << hex << s;
        ss >> hex >> res;
    }

    return res;
}

uint64_t ASMMode::parse_dec(const string& s) const
{
    uint64_t res = 0;

    if(s.at(0) == '0' && s.at(1) == 'x')
        cerr << "Warning: 0x-prefix given, but decimal expected.\n";

    stringstream ss;
    ss << dec << s;
    ss >> dec >> res;

    return res;
}

bool operator==(const AddressRange& lhs, const AddressRange& rhs)
{
    if(lhs.path_ == rhs.path_)
    {
        assert(lhs.low_ == rhs.low_ && lhs.high_ == rhs.high_);
        return true;
    }
    return false;
}

bool operator==(const AddressRange& lhs, const filesystem::path& rhs)
{
    return lhs.path_ == rhs;
}

bool operator==(const filesystem::path& lhs, const AddressRange& rhs)
{
    return rhs == lhs;
}

bool operator==(const AddressRange& lhs, const uint64_t& rhs)
{
    return lhs.low_ <= rhs && lhs.high_ >= rhs;
}

bool operator==(const uint64_t& lhs, const AddressRange& rhs)
{
    return rhs == lhs;
}

} // namespace crete
