/*! @file
 *
 * Author(s): Christopher Havlicek
 * See contacts.txt for contact information.
 */
#ifndef CRETE_TRACE_ANALYZER_H
#define CRETE_TRACE_ANALYZER_H

#include <set>
#include <map>

#include <boost/filesystem/path.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/unordered_map.hpp>
#include <boost/property_tree/ptree.hpp>

#include <crete/dll.h>
#include <crete/trace.h>
#include <crete/elf_reader.h>
#include <crete/addr_range.h>
#include <crete/factory.h>
#include <crete/trace_graph.h>
#include <crete/selector.h>

namespace crete
{

class CRETE_DLL_EXPORT TraceAnalyzer
{
public:
    typedef std::set<Trace::ID> TraceIdSet;
    typedef std::set<Trace> TraceSet;
    typedef std::set<uint64_t> BlockSet;
    typedef uint64_t Weight;
    typedef double Score;
    typedef boost::unordered_map<Trace::Block, Weight> WeightMap;
    typedef std::map<Trace, Score> TraceScoreMap;

public:
    TraceAnalyzer(const std::string selection_strat);
    TraceAnalyzer(const TraceAnalyzer& other);

    auto operator=(const TraceAnalyzer& other) -> TraceAnalyzer&;

    bool insert_trace(const boost::filesystem::path& path); // Returns true if trace was valid (successfully inserted).
    void insert_callback(std::function<void(Trace::ID)> cb); // Calls cb with trace that has been made redundant by newly inserted trace (when the newly inserted trace supercedes cb).
    boost::optional<Trace> next();
    boost::optional<Trace> next_with_unexecuted_blocks();
    void submit_executed(const Trace& trace);

    void print_graph(bool only_branches, std::map<AddressRange, Entry>& elf_entries) const; // Debugging.
    size_t blocks_discovered_count() const;

    void set_selection_strategy(const std::string& strat);
    void set(const trace::SelectionStrategy& strat);
    auto compress_traces(bool b) -> void;

protected:
    void initialize_log();
    void submit_to_unexecuted_block_registry(const Trace& trace);
    void update_global_block_weights(const Trace& trace);
    void update_traces_with_unexecuted_blocks();
    bool contains_unexecuted(const Trace::Blocks& blocks);
    void initialize_selector_factory();

private:
    TraceGraph trace_graph_;
    TraceSet traces_with_unexecuted_blocks_;
    WeightMap global_block_weights_; // Currently only used for traces_with_unexecuted_blocks_. May have a use in the future. We don't need a "weight" for traces_with_unexecuted_blocks_, we just need true/false whether a given block was executed or not.
    Factory<trace::SelectionStrategy, boost::shared_ptr<trace::Selector>> selector_factory_;
    boost::shared_ptr<trace::Selector> trace_selector_;
    trace::SelectionStrategy strat_;
    bool compress_traces_ = false;
};

Trace parse_trace(const boost::filesystem::path& path);

} // namespace crete

#endif // CRETE_TRACE_ANALYZER_H
