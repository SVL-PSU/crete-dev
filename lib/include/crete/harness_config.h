#ifndef CRETE_HARNESS_CONFIG_H
#define CRETE_HARNESS_CONFIG_H

#include <string>
#include <vector>
#include <set>

#include <crete/asio/client.h>
#include <crete/elf_reader.h>
#include <crete/proc_reader.h>
#include <crete/exception.h>
#include <crete/common.h>

#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>

namespace crete
{
namespace config
{

struct File
{
    boost::filesystem::path path;
    uint64_t size;
    bool concolic;
    std::vector<uint8_t> data;

    File() :
        path(boost::filesystem::path()),
        size(0),
        concolic(false),
        data(std::vector<uint8_t>())
    {
    }

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        (void)version;

        std::string path = this->path.generic_string();
        ar & BOOST_SERIALIZATION_NVP(path);
        ar & BOOST_SERIALIZATION_NVP(size);
        ar & BOOST_SERIALIZATION_NVP(concolic);
        ar & BOOST_SERIALIZATION_NVP(data);
    }

    template <class Archive>
    void load(Archive& ar, const unsigned int version)
    {
        (void)version;
        std::string path;

        ar & BOOST_SERIALIZATION_NVP(path);
        ar & BOOST_SERIALIZATION_NVP(size);
        ar & BOOST_SERIALIZATION_NVP(concolic);
        ar & BOOST_SERIALIZATION_NVP(data);

        this->path = boost::filesystem::path(path);
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

typedef std::vector<File> Files;

struct Argument
{
    uint64_t index;
    uint64_t size;
    std::string value;
    bool concolic;

    Argument() :
        index(0),
        size(0),
        value(std::string()),
        concolic(false)
    {
    }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & BOOST_SERIALIZATION_NVP(index);
        ar & BOOST_SERIALIZATION_NVP(size);
        ar & BOOST_SERIALIZATION_NVP(value);
        ar & BOOST_SERIALIZATION_NVP(concolic);
    }
};

typedef std::vector<Argument> Arguments;

struct ArgMinMax
{
    ArgMinMax() :
        concolic(false),
        min(0),
        max(0)
    {
    }

    bool concolic;
    uint32_t min;
    uint32_t max;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & BOOST_SERIALIZATION_NVP(concolic);
        ar & BOOST_SERIALIZATION_NVP(min);
        ar & BOOST_SERIALIZATION_NVP(max);
    }
};

struct STDStream
{
    uint64_t size;
    std::string value;
    bool concolic;

    STDStream() :
        size(0),
        value(std::string()),
        concolic(false)
    {
    }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & BOOST_SERIALIZATION_NVP(size);
        ar & BOOST_SERIALIZATION_NVP(value);
        ar & BOOST_SERIALIZATION_NVP(concolic);
    }
};

typedef boost::filesystem::path Executable;

class HarnessConfiguration
{
    friend class RunConfiguration;
public:
    HarnessConfiguration();
    ~HarnessConfiguration();

    HarnessConfiguration(const boost::filesystem::path xml_file);

    // crete-runner
    void load_file_data();
    void clear_file_data();
    void is_first_iteration(bool b);

    // config-generator
    void set_executable(const Executable& exe);
    void add_argument(const Argument& arg);
    void remove_last_argument();
    void set_argument(const Argument& arg);
    void add_file(const File& file);
    void set_stdin(const STDStream& stdin);

    // guest-preload.cpp
    bool is_first_iteration() const;

    // misc
    Executable get_executable() const;
    Arguments get_arguments() const;
    Files get_files() const;
    STDStream get_stdin() const;

    void write(boost::property_tree::ptree& config) const;

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const;
    template <class Archive>
    void load(Archive& ar, const unsigned int version);
    BOOST_SERIALIZATION_SPLIT_MEMBER()

protected:
    // For parsing crete.xml
    void load_configuration(const boost::filesystem::path xml_file);
    void load_executable(const boost::property_tree::ptree& config_tree);
    void load_arguments(const boost::property_tree::ptree& config_tree);
    void load_argument(const boost::property_tree::ptree& config_tree);
    void load_files(const boost::property_tree::ptree& config_tree);
    void load_file(const boost::property_tree::ptree& config_tree);
    void load_stdin(const boost::property_tree::ptree& config_tree);

    void verify() const;

private:
    Executable executable_;

    Arguments arguments_;
    Files files_;
    STDStream stdin_;

    ArgMinMax argminmax_; // TODO: xxx For supporting variable number of symbolic arguments
    bool first_iteration_;
};

template <class Archive>
void HarnessConfiguration::save(Archive& ar, const unsigned int version) const
{
    (void)version;

    std::string str_executable = executable_.generic_string();

    ar & BOOST_SERIALIZATION_NVP(str_executable);

    ar & BOOST_SERIALIZATION_NVP(arguments_);
    ar & BOOST_SERIALIZATION_NVP(files_);
    ar & BOOST_SERIALIZATION_NVP(stdin_);

    ar & BOOST_SERIALIZATION_NVP(argminmax_);
    ar & BOOST_SERIALIZATION_NVP(first_iteration_);
}

template <class Archive>
void HarnessConfiguration::load(Archive& ar, const unsigned int version)
{
    (void)version;

    std::string str_executable;

    ar & BOOST_SERIALIZATION_NVP(str_executable);

    ar & BOOST_SERIALIZATION_NVP(arguments_);
    ar & BOOST_SERIALIZATION_NVP(files_);
    ar & BOOST_SERIALIZATION_NVP(stdin_);

    ar & BOOST_SERIALIZATION_NVP(argminmax_);
    ar & BOOST_SERIALIZATION_NVP(first_iteration_);

    executable_ = boost::filesystem::path(str_executable);
}

inline
HarnessConfiguration::HarnessConfiguration() :
    executable_(Executable()),
    arguments_(std::vector<Argument>()),
    files_(std::vector<File>()),
    stdin_(STDStream()),
    argminmax_(ArgMinMax()),
    first_iteration_(false)
{
}

inline
HarnessConfiguration::HarnessConfiguration(const boost::filesystem::path xml_file) :
    executable_(Executable()),
    arguments_(std::vector<Argument>()),
    files_(std::vector<File>()),
    stdin_(STDStream()),
    argminmax_(ArgMinMax()),
    first_iteration_(false)
{
    load_configuration(xml_file);
    verify();
}

inline
HarnessConfiguration::~HarnessConfiguration()
{
}

inline
void HarnessConfiguration::load_configuration(const boost::filesystem::path xml_file)
{
    if(!boost::filesystem::exists(xml_file))
    {
        throw std::runtime_error("[CRETE] configuration error - input xml file path is invalid: "
                + xml_file.string());
    }

    boost::property_tree::ptree config_tree;
    boost::property_tree::read_xml(xml_file.string(), config_tree);

    boost::optional<boost::property_tree::ptree&> opt_crete = config_tree.get_child_optional("crete");
    if(!opt_crete)
    {
        throw std::runtime_error("[CRETE] configuration error - missing root node 'crete'");
    }

    const boost::property_tree::ptree& crete_tree = *opt_crete;

    load_executable(crete_tree);
    load_arguments(crete_tree);
    load_files(crete_tree);
    load_stdin(crete_tree);
}

// Parsing rules: Required and must exist
inline
void HarnessConfiguration::load_executable(const boost::property_tree::ptree& config_tree)
{
    namespace fs = boost::filesystem;

    executable_ = config_tree.get<fs::path>("exec");

    if(!fs::exists(executable_))
        throw std::runtime_error("failed to find executable: " + executable_.generic_string());
}

inline
void HarnessConfiguration::load_file_data()
{
    for(Files::iterator it = files_.begin();
        it != files_.end();
        ++it)
    {
        assert(it->data.empty());

        File& file = *it;
        try {
            // Exists and is regular file: read the whole file into file.data
            uintmax_t real_file_size = boost::filesystem::file_size(file.path);
            assert(real_file_size == file.size);

            std::ifstream ifs(file.path.string().c_str());
            file.data.reserve(file.size);
            file.data.assign(std::istreambuf_iterator<char>(ifs),
                                std::istreambuf_iterator<char>());
            assert(file.data.size() == file.size);
        }
        catch(const std::exception& e)
        {
            // Not-exist: use default value '\0's for concolic file
            if(file.concolic)
            {
                file.data.resize(file.size, 0);
            }
            else
            {
                std::cerr << boost::current_exception_diagnostic_information() << std::endl;
                BOOST_THROW_EXCEPTION(e);
            }
        }
    }
}

inline
void HarnessConfiguration::clear_file_data()
{
    for(Files::iterator it = files_.begin();
        it != files_.end();
        ++it)
    {
        File& file = *it;

        file.data.clear();
    }
}

inline
Executable HarnessConfiguration::get_executable() const
{
    return executable_;
}

inline
Arguments HarnessConfiguration::get_arguments() const
{
    return arguments_;
}

inline
Files HarnessConfiguration::get_files() const
{
    return files_;
}

inline
void HarnessConfiguration::write(boost::property_tree::ptree& config) const
{
    verify();

    config.put("crete.exec", executable_.string());

    if(!arguments_.empty())
    {
        boost::property_tree::ptree& args_node = config.put_child("crete.args", boost::property_tree::ptree());
        BOOST_FOREACH(const Argument& arg, arguments_)
        {
            boost::property_tree::ptree& arg_node = args_node.add_child("arg", boost::property_tree::ptree());
            arg_node.put("<xmlattr>.index", arg.index);
            arg_node.put("<xmlattr>.size", arg.size);
            arg_node.put("<xmlattr>.value", arg.value.data());
            arg_node.put("<xmlattr>.concolic", arg.concolic);
        }
    }

    if(!files_.empty())
    {
        boost::property_tree::ptree& files_node = config.put_child("crete.files", boost::property_tree::ptree());
            BOOST_FOREACH(const File& file, files_)
            {
                boost::property_tree::ptree& file_node = files_node.add_child("file", boost::property_tree::ptree());
                file_node.put("<xmlattr>.path", file.path.string());
                file_node.put("<xmlattr>.size", file.size);
                file_node.put("<xmlattr>.concolic", file.concolic);
            }
    }

    if(stdin_.size != 0) {
        boost::property_tree::ptree& stdin_node = config.put_child("crete.stdin", boost::property_tree::ptree());
        stdin_node.put("<xmlattr>.size", stdin_.size);
        stdin_node.put("<xmlattr>.value", stdin_.value.data());
        stdin_node.put("<xmlattr>.concolic", stdin_.concolic);
    }
}

inline
void HarnessConfiguration::set_executable(const Executable& exe)
{
    assert(!exe.empty());
    executable_ = exe;
}

inline
void HarnessConfiguration::add_argument(const Argument& arg)
{
    assert(arg.index == (arguments_.size() + 1));
    arguments_.push_back(arg);
}

inline
void HarnessConfiguration::remove_last_argument()
{
    arguments_.pop_back();
}

inline
void HarnessConfiguration::set_argument(const Argument& arg)
{
    for(Arguments::iterator it = arguments_.begin();
            it != arguments_.end(); ++it) {
        if(it->index != arg.index)
            continue;

        (*it) = arg;
        return;
    }

    assert(0 && "No existing argument found with matched index.\n");
}

inline
void HarnessConfiguration::add_file(const File& file)
{
    files_.push_back(file);
}

inline
void HarnessConfiguration::set_stdin(const STDStream& stdin)
{
    stdin_ = stdin;
}

inline
bool HarnessConfiguration::is_first_iteration() const
{
    return first_iteration_;
}

inline
void HarnessConfiguration::is_first_iteration(bool b)
{
    first_iteration_ = b;
}

inline
void HarnessConfiguration::load_arguments(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const boost::property_tree::ptree&> opt_args = config_tree.get_child_optional("args");

    if(!opt_args)
    {
        return;
    }

    BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                  *opt_args)
    {
        load_argument(v.second);
    }
}

// Format: index => int, value => string, size => int, concolic => bool
// Rules: Either 'value' or 'size' is required. Size is valid only if its
//        value is bigger than the length of "value'. Default value for 'value'
//        is set of '\0's. Concolic is optional with default value 'false'.
inline
void HarnessConfiguration::load_argument(const boost::property_tree::ptree& config_tree)
{
    namespace fs = boost::filesystem;

    Argument arg;

    // TODO: xxx remove index from crete.xml, while keeping it as internal usage
    // NOTE:        keep in mind argv[0]
    arg.index = config_tree.get<uint64_t>("<xmlattr>.index");
    arg.size = config_tree.get<uint64_t>("<xmlattr>.size", 0);
    arg.value = config_tree.get<std::string>("<xmlattr>.value", "");
    arg.concolic = config_tree.get<bool>("<xmlattr>.concolic", false);

    if(arg.size == 0 && arg.value.empty())
    {
        throw std::runtime_error("size is 0 and value is empty for arg");
    }

    if(arg.size > arg.value.size())
    {
        arg.value.resize(arg.size, 0);
    }
    arg.size = arg.value.size();

    // NOTE: xxx exclude argv[0] from crete-guest-config while parsing
    //       crete-replay and crete-run utilizes this assumption
    if(arg.index != 0)
    {
        arguments_.push_back(arg);
    }
}

inline
void HarnessConfiguration::load_files(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const boost::property_tree::ptree&> opt_files = config_tree.get_child_optional("files");

    if(!opt_files)
    {
        return;
    }

    BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                  *opt_files)
    {
        load_file(v.second);
    }
}

// Format: path => string, concolic => bool, size => int
// Parsing Rules:
//      1. Path is required, can be relative to executable or absolute.
//      2. Concolic is optional with default value "false"
//      3. Non-concolic files must exist and size given is omitted
//      4. Conoclic files are optional with default contents of "\0"s.
//         Given "size" is valid only if concolic file does not exists
inline
void HarnessConfiguration::load_file(const boost::property_tree::ptree& config_tree)
{
    namespace fs = boost::filesystem;

    assert(!executable_.empty() && "[crete-config error] executable_ must be initialized "
            "before initializing files_\n");

    File file;

    file.path = config_tree.get<fs::path>("<xmlattr>.path");
    // Make relative path absolute
    if(file.path.string().find('/') != 0) {
        file.path = executable_ / file.path;
    }

    file.concolic= config_tree.get<bool>("<xmlattr>.concolic", false);

    try {
        // Exsits and is regular file
        file.size = boost::filesystem::file_size(file.path);
    }
    catch(const std::exception& e)
    {
        // Not exsits
        if(file.concolic)
        {
            file.size = config_tree.get<uint64_t>("<xmlattr>.size");;
        }
        else
        {
            std::cerr << boost::current_exception_diagnostic_information() << std::endl;
            BOOST_THROW_EXCEPTION(e);
        }
    }

    assert(file.size != 0);

    files_.push_back(file);

    // Append full path of the file to the argument list
    Argument arg;

    arg.index = arguments_.size() + 1;
    if(file.concolic)
    {
        arg.value = fs::path(CRETE_RAMDISK_PATH / file.path.filename()).string();
    } else {
        arg.value = file.path.string();
    }
    arg.size = arg.value.size();
    arg.concolic = false;

    arguments_.push_back(arg);
}

inline
STDStream HarnessConfiguration::get_stdin() const
{
    return stdin_;
}

// Format: size => int, value => string, concolic => bool
// Parsing Rules:
//          1. Either 'value' or 'size' is required.
//          2. Concolic is optional with default value 'false'.
//          3. Size is valid only if its value is bigger than the length of "value'.
//          4. Default value for 'value' is '\0's.
inline
void HarnessConfiguration::load_stdin(const boost::property_tree::ptree& config_tree)
{
    boost::optional<const boost::property_tree::ptree&> opt_stdin = config_tree.get_child_optional("stdin");

    if(!opt_stdin)
    {
        return;
    }

    const boost::property_tree::ptree& stdin_config = *opt_stdin;

    stdin_.size = stdin_config.get<uint64_t>("<xmlattr>.size", 0);
    stdin_.value = stdin_config.get<std::string>("<xmlattr>.value", "");
    stdin_.concolic = stdin_config.get<bool>("<xmlattr>.concolic", false);

    assert(stdin_.size > 0 || !stdin_.value.empty());

    if(stdin_.size > stdin_.value.size())
    {
        stdin_.value.resize(stdin_.size);
    }
    stdin_.size = stdin_.value.size();
}

inline
void HarnessConfiguration::verify() const
{
    CRETE_EXCEPTION_ASSERT(!executable_.empty(),
            err::msg("Invalid crete config file: empty executable path.\n"));

    std::vector<bool> index_array;
    index_array.resize(arguments_.size(), false);
    for(Arguments::const_iterator it = arguments_.begin();
            it != arguments_.end(); ++it) {
        if(it->size != it->value.size())
        {
            std::cerr << "arg[" << std::dec << it->index << "]"
                    << ", size = " << it->size
                    << ", value_size = " << it->value.size()
                    << ", value = " << it->value << std::endl;

            CRETE_EXCEPTION_THROW(err::msg("Invalid crete config file: inconsistent argument value and size\n"));
        }

        if(it->index > arguments_.size())
        {
            std::cerr << "arg[" << std::dec << it->index << "]"
                    << ", size = " << it->size
                    << ", value_size = " << it->value.size()
                    << ", value = " << it->value << std::endl;

            CRETE_EXCEPTION_THROW(err::msg("Invalid crete config file: invalid argument index!\n"));
        }

        if(it->index == 0)
            continue;

        if(index_array[it->index - 1] == true) {
            std::cerr << "arg[" << std::dec << it->index << "]"
                    << ", size = " << it->size
                    << ", value_size = " << it->value.size()
                    << ", value = " << it->value << std::endl;

            CRETE_EXCEPTION_THROW(err::msg("Invalid crete config file: duplicated argument index!\n"));
        }
        index_array[it->index - 1] = true;
    }

    std::set<std::string> set_filename;
    for(Files::const_iterator it = files_.begin();
            it != files_.end(); ++it) {
        CRETE_EXCEPTION_ASSERT(!it->path.empty(),
                err::msg("Invalid crete config file: empty file path!\n"));
        CRETE_EXCEPTION_ASSERT(set_filename.insert(it->path.filename().string()).second,
                err::msg("Invalid crete config file: multiple files using the same name!\n"));
    }

    assert(stdin_.size == stdin_.value.size() &&
            "crete configuration error: inconsistent stdin value and size\n");
}

} // namespace config
} // namespace crete

#endif // CRETE_HARNESS_CONFIG_H
