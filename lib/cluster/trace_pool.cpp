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

TracePool::TracePool(const option::Dispatch& options)
    : options_(options), trace_count_(0) {}

auto TracePool::insert(const TracePath& trace) -> bool
{
    if(options_.trace.print_elf_info)
    {
        print_elf_info(trace);
    }

    ++trace_count_;
    next_.push_front(trace);

    return true;
}

auto TracePool::next() -> optional<TracePath>
{
    if(next_.empty())
    {
        return boost::optional<TracePath>{};
    }

    optional<TracePath> trace = optional<TracePath>(next_.back());
    next_.pop_back();

    return trace;
}

auto TracePool::count_all_unique() const -> size_t
{
    return trace_count_;
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

// TODO: xxx From trace_analyzer.cpp
static Trace parse_trace(const boost::filesystem::path& path)
{
    filesystem::ifstream ifs(path, ios_base::in | ios_base::binary);
    if(!ifs.good())
        throw runtime_error("failed to open file: " + path.generic_string());

    Trace::Blocks blocks;

    auto block_addr = uint64_t{0};
    while(ifs.read(reinterpret_cast<char*>(&block_addr), sizeof(uint64_t)))
        blocks.push_back(block_addr);

    return Trace{path.parent_path().generic_string(), blocks};
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
} // namespace cluster
} // namespace crete
