#include <crete/cluster/trace_pool.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <iomanip>
#include <sstream>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/variate_generator.hpp>

using namespace std;
using namespace boost;
namespace fs = boost::filesystem;

namespace crete
{
namespace cluster
{

TracePool::TracePool(const option::Dispatch& options,
                     const string& selection_strat)
    : trace_analyzer_(selection_strat)
    , random_engine_(std::time(0)) // Random seed based on execution time.
    , options_(options)
{
    trace_analyzer_.insert_callback([this](Trace::ID id)
    {
        // TODO: temporarily disable. For some reason, calling remove_trace(id) via callback by TraceGraph (sometimes) causes a segfault. It's safe to ignore, but next_ will never be empty.
//        remove_trace(id);
    });
    set_selection_strategy(selection_strat);
    trace_analyzer_.compress_traces(options.trace.compress);
}

bool TracePool::remove_trace(const Trace::ID& id)
{
    auto it = find(next_.begin(),
                   next_.end(),
                   filesystem::path(id));

    if(it == next_.end())
        return false;

    next_.erase(it);

    return true;
}

void TracePool::print_elf_info(const std::set<fs::path>& traces)
{
    for(std::set<fs::path>::const_iterator it = traces.begin();
        it != traces.end();
        ++it)
    {
        print_elf_info(*it);
    }
}

void TracePool::print_elf_info(const filesystem::path& trace_path)
{
    fs::path bin_seq = trace_path / "tb-seq.bin";
    fs::path elf_seq = trace_path / "tb-seq-elf.txt";

    fs::path pm_path = "guest-data/proc_maps.log";

    if(!fs::exists(pm_path))
    {
        throw runtime_error("failed to open file: " + pm_path.string());
    }

    ProcReader pr(pm_path);
    ProcMaps pms = condense(pr.find_all());

    Trace trace = parse_trace(bin_seq);

    fs::ofstream ofs(elf_seq);

    ofs << std::hex;
    ofs << std::left;

    if(!ofs.good())
        throw std::runtime_error("failed to open file: " + elf_seq.string());

    const Trace::Blocks& blocks = trace.get_blocks();

    size_t block_counter = 0;
    for(Trace::Blocks::const_iterator it = blocks.begin();
        it != blocks.end();
        ++it)
    {
        const Trace::Block& block = *it;

        uint64_t offset = 0;
        std::string lib_path;

        for(ProcMaps::const_iterator pit = pms.begin();
            pit != pms.end();
            ++pit)
        {
            const ProcMap& pm = *pit;

            if(is_in(AddressRange(pm.address().first, pm.address().second),
                     block))
            {
                if(pm.path() == pr.get_executable())
                {
                    offset = block;
                }
                else
                {
                    offset = block - pm.address().first;
                }

                lib_path = pm.path();

                break;
            }
        }

        std::string func_name;

        Entry entry;
        entry.addr = block;
        std::set<Entry>::const_iterator eit = elf_entry_set_.find(entry);
        if(eit != elf_entry_set_.end())
        {
            func_name = eit->name;
        }
        else
        {
            for(std::map<AddressRange, Entry>::const_iterator it = elf_entries_.begin();
                it != elf_entries_.end();
                ++it)
            {
                if(is_in(it->first, block))
                {
                    func_name = it->second.name;
                    break;

                }
            }
        }

        stringstream ssblock_counter;
        ssblock_counter << block_counter << ": ";

        ofs << setw(6)  << ssblock_counter.str()
            << setw(14) << block
            << " -> "
            << setw(14) << offset
            << " -> "
            << setw(35) << func_name
            << " -> "
            << setw(45) << lib_path
            << '\n';

        ++block_counter;
    }
}

auto TracePool::insert(const TracePath& trace) -> bool
{
    all_.insert(trace);

    if(options_.trace.print_elf_info)
    {
        print_elf_info(trace);
    }

    if(options_.trace.filter_traces)
    {
        if(trace_analyzer_.insert_trace(trace))
        {
            all_unique_.insert(trace);
            next_.insert(trace);

            return true;
        }
    }

    return false;
}

auto TracePool::next() -> optional<TracePool::TracePath>
{
    optional<TracePath> trace;

    // TODO: replace opt_prefer_untreaded_paths_ with "<hueristic></hueristic>" from guest config.
//    if(opt_prefer_untreaded_paths_)
//    {
//        optional<Trace> tmp = trace_analyzer_.next_with_unexecuted_blocks();
//        if(tmp)
//        {
////            std::cout << "selected: next_with_unexecuted_blocks" << std::endl;

//            trace = optional<TracePath>(tmp->get_id());
//            TracePathSet::const_iterator it = pool_.find(*trace);
//            assert(it != pool_.end());
//            pool_.erase(it);
//        }
//    }
    if(!trace)
    {
        optional<Trace> tmp = trace_analyzer_.next();
        if(tmp)
        {
            trace = optional<TracePath>(tmp->get_id());
            TracePathSet::const_iterator it = next_.find(*trace);
            assert(it != next_.end());
            next_.erase(it);
        }
    }

    if(trace)
    {
        trace_analyzer_.submit_executed(parse_trace((*trace)/"tb-seq.bin"));
    }

    if(options_.trace.print_trace_selection)
    {
        if(trace)
            std::cout << "next: " << trace->generic_string() << std::endl;
    }
    if(options_.trace.print_graph)
    {
        trace_analyzer_.print_graph(options_.trace.print_graph_only_branches,
                                    elf_entries_);
    }

    return trace;
}

auto TracePool::count_all_unique() const -> size_t
{
    return all_unique_.size();
}

auto TracePool::count_all() const -> size_t
{
    return all_.size();
}

auto TracePool::count_next() const -> size_t
{
    return next_.size();
}

void TracePool::set(const std::map<AddressRange, Entry>& entries)
{
    elf_entries_ = entries;

    for(std::map<AddressRange, Entry>::const_iterator it = entries.begin();
        it != entries.end();
        ++it)
    {
        if(elf_entry_set_.insert(it->second).second == false)
        {
            throw std::runtime_error("found duplicate elf entry: " + it->second.name);
        }
    }
}

size_t TracePool::blocks_discovered_count() const
{
    return trace_analyzer_.blocks_discovered_count();
}

void TracePool::set_selection_strategy(const string& strat)
{
    trace_analyzer_.set_selection_strategy(strat);
}

} // namespace cluster
} // namespace crete
