#ifndef CRETE_CLUSTER_DISPATCH_OPTIONS_H
#define CRETE_CLUSTER_DISPATCH_OPTIONS_H

#include <string>
#include <vector>

#include <crete/test_case.h>

namespace crete
{
namespace cluster
{
namespace option
{

struct Mode
{
    std::string name;
    bool distributed{false};

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & name;
        ar & distributed;
    }
};

struct Interval
{
    uint64_t trace{0};
    uint64_t tc{0};
    uint64_t time{0};

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & trace;
        ar & tc;
        ar & time;
    }
};

struct Test
{
    typedef std::vector<std::string> Items;
    typedef std::vector<std::string> Seeds;

    Interval interval;
    Items items;
    Seeds seeds;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & interval;
        ar & items;
        ar & seeds;
    }
};

struct Image
{
    std::string path;
    bool update{false};

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & path;
        ar & update;
    }
};

struct VM
{
    Image image;
    std::string arch;
    std::string snapshot;
    std::string args;
    TestCase initial_tc;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & image;
        ar & arch;
        ar & snapshot;
        ar & args;
        ar & initial_tc;
    }
};

struct SVM
{
    struct Args
    {
        std::string concolic;
        std::string symbolic;

        template <class Archive>
        void serialize(Archive& ar, const unsigned int version)
        {
            (void)version;

            ar & concolic;
            ar & symbolic;
        }
    } args;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & args;
    }
};

struct Trace
{
    bool filter_traces{true};
    bool print_trace_selection{false};
    bool print_graph{false};
    bool print_graph_only_branches{false}; // TODO: Now redundant. We only dump 'branches.'
    bool print_elf_info{false};
    bool compress{false};

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & filter_traces;
        ar & print_trace_selection;
        ar & print_graph;
        ar & print_graph_only_branches;
        ar & print_elf_info;
        ar & compress;
    }
};

struct Profile
{
    uint32_t interval;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & interval;
    }
};

struct Dispatch
{
    Mode mode;
    VM vm;
    SVM svm;
    Test test;
    Trace trace;
    Profile profile;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & mode;
        ar & vm;
        ar & svm;
        ar & test;
        ar & trace;
        ar & profile;
    }
};

} // namespace option
} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_DISPATCH_OPTIONS_H
