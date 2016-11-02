#ifndef CRETE_TRACE_POOL_H
#define CRETE_TRACE_POOL_H

#include <set>

#include <boost/filesystem/path.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/random/mersenne_twister.hpp>

#include <crete/trace_analyzer.h>
#include <crete/elf_reader.h>
#include <crete/addr_range.h>
#include <crete/proc_reader.h>
#include <crete/cluster/dispatch_options.h>

namespace crete
{
namespace cluster
{
    class TracePool
    {
    public:
        using TracePath = boost::filesystem::path;
        using TracePathSet = std::set<TracePath>;

    public:
        TracePool(const option::Dispatch& options,
                  const std::string& selection_strat);

        auto insert(const TracePath& tace) -> bool;
        auto next() -> boost::optional<TracePath>;
        auto count_all() const -> size_t;
        auto count_all_unique() const -> size_t;
        auto count_next() const -> size_t;
        auto set(const std::map<AddressRange, Entry>& entries) -> void;
        auto blocks_discovered_count() const -> size_t;
        auto set_selection_strategy(const std::string& strat) -> void;

    protected:
        auto remove_trace(const crete::Trace::ID& id) -> bool;
        void print_elf_info(const std::set<boost::filesystem::path>& traces);
        void print_elf_info(const boost::filesystem::path& trace_path);

    private:
        TraceAnalyzer trace_analyzer_;
        TracePathSet all_;
        TracePathSet all_unique_;
        TracePathSet next_;
        boost::random::mt19937 random_engine_;
        std::map<AddressRange, Entry> elf_entries_;
        std::set<Entry> elf_entry_set_;
        option::Dispatch options_;
    };
} // namespace cluster
} // namespace crete

#endif // CRETE_TRACE_POOL_H
