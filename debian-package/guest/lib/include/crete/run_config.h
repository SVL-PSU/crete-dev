#ifndef CRETE_GUEST_RUN_CONFIG_H
#define CRETE_GUEST_RUN_CONFIG_H

#include <string>
#include <vector>
#include <sstream>

#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/functional/hash.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "crete/harness_config.h"

namespace crete
{
namespace config
{

struct Preload
{
    boost::filesystem::path lib;

    Preload() :
        lib(boost::filesystem::path())
    {
    }

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        (void)version;

        std::string lib = this->lib.generic_string();

        ar & BOOST_SERIALIZATION_NVP(lib);
    }
    template <class Archive>
    void load(Archive& ar, const unsigned int version)
    {
        (void)version;

        std::string lib;

        ar & BOOST_SERIALIZATION_NVP(lib);

        this->lib = boost::filesystem::path(lib);
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

typedef std::vector<Preload> Preloads;

struct Function
{
    std::string name;
    boost::filesystem::path lib;

    Function() :
        name(std::string()),
        lib(boost::filesystem::path())
    {
    }

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        (void)version;

        std::string lib = this->lib.generic_string();

        ar & BOOST_SERIALIZATION_NVP(name);
        ar & BOOST_SERIALIZATION_NVP(lib);
    }
    template <class Archive>
    void load(Archive& ar, const unsigned int version)
    {
        (void)version;

        std::string lib;

        ar & BOOST_SERIALIZATION_NVP(name);
        ar & BOOST_SERIALIZATION_NVP(lib);

        this->lib = boost::filesystem::path(lib);
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

typedef std::vector<Function> Functions;

struct CodeSelectionBounds
{
    int32_t low;
    int32_t high;

    CodeSelectionBounds() :
        low(0),
        high(0)
    {
    }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & BOOST_SERIALIZATION_NVP(low);
        ar & BOOST_SERIALIZATION_NVP(high);
    }
};

struct Exploration
{
    uint32_t call_depth;
    CodeSelectionBounds stack_depth;
    std::string strategy;
    // std::vector<Heuristic> heuristics;

    Exploration() :
        call_depth(0),
        stack_depth(CodeSelectionBounds()),
        strategy(std::string())
    {
    }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & BOOST_SERIALIZATION_NVP(call_depth);
        ar & BOOST_SERIALIZATION_NVP(stack_depth);
        ar & BOOST_SERIALIZATION_NVP(strategy);
    }

};

class RunConfiguration : public HarnessConfiguration
{
public:
    typedef std::string Library; // TODO: is there a good reason the type isn't fs::path? Serialization, but I've overcome that problem.
    typedef std::vector<Library> Libraries;
    typedef std::vector<std::string> SectionExclusions;

public:
    RunConfiguration();

    // For parsing crete.xml
    RunConfiguration(const boost::filesystem::path xml_file);

    Preloads get_preloads() const;
    Functions get_include_functions() const;
    Functions get_exclude_functions() const;
    Libraries get_libraries() const;
    Exploration get_exploration() const;
    SectionExclusions get_section_exclusions() const;

    void load_preloads(const boost::property_tree::ptree& config_tree);
    void load_preload(const boost::property_tree::ptree& config_tree);
    void load_functions(const boost::property_tree::ptree& config_tree);
    void load_function(const boost::property_tree::ptree& config_tree, Functions& functions);
    void load_libraries(const boost::property_tree::ptree& config_tree);
    void load_exploration(const boost::property_tree::ptree& config_tree);
    void load_call_depth(const boost::property_tree::ptree& config_tree);
    void load_stack_depth(const boost::property_tree::ptree& config_tree);
    void load_strategy(const boost::property_tree::ptree& config_tree);
    void load_sections(const boost::property_tree::ptree& config_tree);

    void write(boost::property_tree::ptree& config) const;
    void write_mini(boost::property_tree::ptree& config) const;

    template <class Archive>
    void save(Archive& ar, const unsigned int version) const;
    template <class Archive>
    void load(Archive& ar, const unsigned int version);
    BOOST_SERIALIZATION_SPLIT_MEMBER()

protected:
    void load_configuration(const boost::filesystem::path xml_file);
    void validate(Exploration& exploration);

private:
    Preloads preloads_;
    Functions include_functions_;
    Functions exclude_functions_;
    Libraries libraries_;
    Exploration exploration_;

    // Internal.
    SectionExclusions section_exclusions_; // i.e., call-stack exclusion.
};

template <class Archive>
void RunConfiguration::save(Archive& ar, const unsigned int version) const
{
    HarnessConfiguration::save(ar, version);

    (void)version;

    ar & BOOST_SERIALIZATION_NVP(preloads_);
    ar & BOOST_SERIALIZATION_NVP(include_functions_);
    ar & BOOST_SERIALIZATION_NVP(exclude_functions_);
    ar & BOOST_SERIALIZATION_NVP(libraries_);
    ar & BOOST_SERIALIZATION_NVP(exploration_);
    ar & BOOST_SERIALIZATION_NVP(section_exclusions_);
}

template <class Archive>
void RunConfiguration::load(Archive& ar, const unsigned int version)
{
    HarnessConfiguration::load(ar, version);

    (void)version;


    ar & BOOST_SERIALIZATION_NVP(preloads_);
    ar & BOOST_SERIALIZATION_NVP(include_functions_);
    ar & BOOST_SERIALIZATION_NVP(exclude_functions_);
    ar & BOOST_SERIALIZATION_NVP(libraries_);
    ar & BOOST_SERIALIZATION_NVP(exploration_);
    ar & BOOST_SERIALIZATION_NVP(section_exclusions_);
}

inline
RunConfiguration::RunConfiguration():
    HarnessConfiguration(),
    preloads_(std::vector<Preload>()),
    include_functions_(std::vector<Function>()),
    exclude_functions_(std::vector<Function>()),
    libraries_(std::vector<Library>()),
    exploration_(Exploration()),
    section_exclusions_(std::vector<std::string>())
{

}

inline
RunConfiguration::RunConfiguration(const boost::filesystem::path xml_file):
    HarnessConfiguration(xml_file),
    preloads_(std::vector<Preload>()),
    include_functions_(std::vector<Function>()),
    exclude_functions_(std::vector<Function>()),
    libraries_(std::vector<Library>()),
    exploration_(Exploration()),
    section_exclusions_(std::vector<std::string>())
{
    RunConfiguration::load_configuration(xml_file);
}

inline
Preloads RunConfiguration::get_preloads() const
{
    return preloads_;
}

inline
void RunConfiguration::load_configuration(const boost::filesystem::path xml_file)
{
    namespace pt = boost::property_tree;

    if(!boost::filesystem::exists(xml_file))
    {
        throw std::runtime_error("[CRETE] configuration error - input xml file path is invalid: "
                + xml_file.string());
    }

    pt::ptree config_tree;
    pt::read_xml(xml_file.string(), config_tree);

    boost::optional<pt::ptree&> opt_crete = config_tree.get_child_optional("crete");
    if(!opt_crete)
    {
        throw std::runtime_error("[CRETE] configuration error - missing root node 'crete'");
    }

    const pt::ptree& crete_tree = *opt_crete;

    load_preloads(crete_tree);
    load_functions(crete_tree);
    load_libraries(crete_tree);
    load_exploration(crete_tree);
    load_sections(crete_tree);
}

inline
void RunConfiguration::validate(Exploration& exploration)
{
    const char* const strategies[] = {
        "bfs",
        "exp-1",
        "exp-2",
        "random",
        "weighted"
    };
    std::size_t strat_count = sizeof(strategies) / sizeof(char**);

    bool found = false;
    for(std::size_t i = 0; i < strat_count; ++i)
    {
        if(exploration.strategy == strategies[i])
        {
            found = true;
        }
    }
    if(!found)
    {
        std::string strats;
        for(std::size_t i = 0; i < strat_count; ++i)
        {
            strats += strategies[i];
            strats += "\n";
        }

        throw std::runtime_error("invalid exploration strategy: "
                                 + exploration.strategy
                                 + "\nPlease select one of: "
                                 + strats);
    }
}

inline
Functions RunConfiguration::get_include_functions() const
{
    return include_functions_;
}

inline
Functions RunConfiguration::get_exclude_functions() const
{
    return exclude_functions_;
}

inline
RunConfiguration::Libraries RunConfiguration::get_libraries() const
{
    return libraries_;
}

inline
Exploration RunConfiguration::get_exploration() const
{
    return exploration_;
}

inline
RunConfiguration::SectionExclusions RunConfiguration::get_section_exclusions() const
{
    return section_exclusions_;
}

inline
void RunConfiguration::load_preloads(const boost::property_tree::ptree& config_tree)
{
    namespace pt = boost::property_tree;

    boost::optional<const pt::ptree&> opt_preloads = config_tree.get_child_optional("preloads");

    if(!opt_preloads)
    {
        return;
    }

    const pt::ptree& preloads = *opt_preloads;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                  preloads)
    {
        load_preload(v.second);
    }
}

inline
void RunConfiguration::load_preload(const boost::property_tree::ptree& config_tree)
{
    Preload preload;

    preload.lib = config_tree.get<std::string>("<xmlattr>.path");

    preloads_.push_back(preload);
}

inline
void RunConfiguration::load_functions(const boost::property_tree::ptree& config_tree)
{
    namespace pt = boost::property_tree;

    boost::optional<const pt::ptree&> opt_funcs = config_tree.get_child_optional("funcs");

    if(!opt_funcs)
    {
        return;
    }

    const pt::ptree& funcs = *opt_funcs;

    if(funcs.get_child_optional("include"))
    {
        BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                      funcs.get_child("include") )
        {
            load_function(v.second, include_functions_);
        }
    }
    if(funcs.get_child_optional("exclude"))
    {
        BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                      funcs.get_child("exclude") )
        {
            load_function(v.second, exclude_functions_);
        }
    }
}

inline
void RunConfiguration::load_function(const boost::property_tree::ptree& config_tree,
                                     Functions& functions)
{
    namespace fs = boost::filesystem;

    Function func;
    func.name = config_tree.get<std::string>("<xmlattr>.name");

    boost::optional<fs::path> opt_lib = config_tree.get_optional<fs::path>("<xmlattr>.lib");
    if(opt_lib)
    {
        func.lib = *opt_lib;
    }

    functions.push_back(func);
}

inline
void RunConfiguration::load_libraries(const boost::property_tree::ptree& config_tree)
{
    namespace pt = boost::property_tree;

    boost::optional<const pt::ptree&> opt_libs = config_tree.get_child_optional("libs");

    if(!opt_libs)
    {
        return;
    }

    const pt::ptree& libs = *opt_libs;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                  libs )
    {
        libraries_.push_back(v.second.get<std::string>("<xmlattr>.path"));
    }
}

inline
void RunConfiguration::load_exploration(const boost::property_tree::ptree& config_tree)
{
    namespace pt = boost::property_tree;

    boost::optional<const pt::ptree&> opt_exploration = config_tree.get_child_optional("exploration");
    if(!opt_exploration)
    {
        return;
    }

    const pt::ptree& exploration = *opt_exploration;

    load_call_depth(exploration);
    load_stack_depth(exploration);
    load_strategy(exploration);
//    load_hueristics(exploration);

    validate(exploration_);
}

inline
void RunConfiguration::load_call_depth(const boost::property_tree::ptree& config_tree)
{
    exploration_.call_depth = config_tree.get<uint32_t>("call-depth", 0);
}

inline
void RunConfiguration::load_stack_depth(const boost::property_tree::ptree& config_tree)
{
    namespace pt = boost::property_tree;

    boost::optional<const pt::ptree&> opt_stack_depth = config_tree.get_child_optional("stack-depth");

    if(opt_stack_depth)
    {
        const pt::ptree& depth = *opt_stack_depth;

        exploration_.stack_depth.low = depth.get<int32_t>("<xmlattr>.low");
        exploration_.stack_depth.high = depth.get<int32_t>("<xmlattr>.high");
    }
}

inline
void RunConfiguration::load_strategy(const boost::property_tree::ptree& config_tree)
{
    exploration_.strategy = config_tree.get<std::string>("strategy", "disabled");
    std::cerr << "[CRETE Warning] Disabled: exploration strategy specified in guest-config.\n";
}

inline
void RunConfiguration::load_sections(const boost::property_tree::ptree& config_tree)
{
    namespace pt = boost::property_tree;

    boost::optional<const pt::ptree&> opt_sections = config_tree.get_child_optional("sections.exclusions");

    if(!opt_sections)
    {
        return;
    }

    const pt::ptree& sections = *opt_sections;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type& v,
                  sections)
    {
        section_exclusions_.push_back(v.second.get_value<std::string>());
    }
}

inline
void RunConfiguration::write(boost::property_tree::ptree& config) const
{
    namespace pt = boost::property_tree;

    HarnessConfiguration::write(config);

    pt::ptree& preloads = config.put_child("crete.preloads", pt::ptree());
    pt::ptree& funcs = config.put_child("crete.funcs", pt::ptree());
    pt::ptree& ifuncs = funcs.put_child("include", pt::ptree());
    pt::ptree& ofuncs = funcs.put_child("exclude", pt::ptree());
    pt::ptree& libs = config.put_child("crete.libs", pt::ptree());
    pt::ptree& exploration = config.put_child("crete.exploration", pt::ptree());

    BOOST_FOREACH(const Preload& preload, preloads_)
    {
        pt::ptree& preload_node = preloads.add_child("preload", pt::ptree());
        preload_node.put("<xmlattr>.path", preload.lib.string());
    }

    BOOST_FOREACH(const Function& func, include_functions_)
    {
        pt::ptree& func_node = ifuncs.add_child("func", pt::ptree());
        func_node.put("<xmlattr>.name", func.name);
        func_node.put("<xmlattr>.lib", func.lib.string());
    }

    BOOST_FOREACH(const Function& func, exclude_functions_)
    {
        pt::ptree& func_node = ofuncs.add_child("func", pt::ptree());
        func_node.put("<xmlattr>.name", func.name);
        func_node.put("<xmlattr>.lib", func.lib.string());
    }

    BOOST_FOREACH(const Library& lib, libraries_)
    {
        pt::ptree& lib_node = libs.add_child("lib", pt::ptree());
        lib_node.put("<xmlattr>.path", lib);
    }

    {
        exploration.put("call-depth", exploration_.call_depth);
        exploration.put("strategy", exploration_.strategy);
    }
}

inline
void RunConfiguration::write_mini(boost::property_tree::ptree& config) const
{
    namespace pt = boost::property_tree;

    HarnessConfiguration::write(config);

    /*
    pt::ptree& preloads = config.put_child("crete.preloads", pt::ptree());
    pt::ptree& funcs = config.put_child("crete.funcs", pt::ptree());
    pt::ptree& ifuncs = funcs.put_child("include", pt::ptree());
    pt::ptree& ofuncs = funcs.put_child("exclude", pt::ptree());
    pt::ptree& libs = config.put_child("crete.libs", pt::ptree());

    BOOST_FOREACH(const Preload& preload, preloads_)
    {
        pt::ptree& preload_node = preloads.add_child("preload", pt::ptree());
        preload_node.put("<xmlattr>.path", preload.lib.string());
    }

    BOOST_FOREACH(const Function& func, include_functions_)
    {
        pt::ptree& func_node = ifuncs.add_child("func", pt::ptree());
        func_node.put("<xmlattr>.name", func.name);
        func_node.put("<xmlattr>.lib", func.lib.string());
    }

    BOOST_FOREACH(const Function& func, exclude_functions_)
    {
        pt::ptree& func_node = ofuncs.add_child("func", pt::ptree());
        func_node.put("<xmlattr>.name", func.name);
        func_node.put("<xmlattr>.lib", func.lib.string());
    }

    BOOST_FOREACH(const Library& lib, libraries_)
    {
        pt::ptree& lib_node = libs.add_child("lib", pt::ptree());
        lib_node.put("<xmlattr>.path", lib);
    }


    pt::ptree& exploration = config.put_child("crete.exploration", pt::ptree());
    {
        exploration.put("call-depth", exploration_.call_depth);
        exploration.put("strategy", exploration_.strategy);
    }
    */
}

} // namespace config
} // namespace crete

#endif // CRETE_GUEST_RUN_CONFIG_H
