#ifndef CRETE_TRACE_POOL_H
#define CRETE_TRACE_POOL_H

#include <set>
#include <deque>

#include <boost/filesystem/path.hpp>
#include <boost/property_tree/ptree.hpp>

#include <crete/trace.h>
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
        using TraceQueue = std::deque<TracePath>;

    public:
        TracePool(const option::Dispatch& options);

        auto insert(const TracePath& tace) -> bool;
        auto next() -> boost::optional<TracePath>;
        auto count_all_unique() const -> size_t;
        auto count_next() const -> size_t;

        // TODO: xxx unused?
        auto set(const std::map<AddressRange, Entry>& entries) -> void;

    protected:
        void print_elf_info(const boost::filesystem::path& trace_path);

    private:
        uint64_t trace_count_;
        TraceQueue next_;
        option::Dispatch options_;

        // TODO: xxx cleanup
        std::map<AddressRange, Entry> elf_entries_;
        std::set<Entry> elf_entry_set_;
    };
} // namespace cluster
} // namespace crete

#endif // CRETE_TRACE_POOL_H
