#include <crete/trace_analyzer.h>
#include <crete/util/cycle.h>

#include <iostream>
#include <functional>

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/make_shared.hpp>


using namespace std;
using namespace boost;

namespace crete
{

TraceAnalyzer::TraceAnalyzer(const string selection_strat)
{
    initialize_selector_factory();
    set_selection_strategy(selection_strat);
}

TraceAnalyzer::TraceAnalyzer(const TraceAnalyzer& other)
{
    // See reasoning in operator= def.
    *this = other;
}

auto TraceAnalyzer::operator=(const TraceAnalyzer& other) -> TraceAnalyzer&
{
    // TODO: redo with copy-and-swap idiom for exception saftey.
    trace_graph_ = other.trace_graph_;
    traces_with_unexecuted_blocks_ = other.traces_with_unexecuted_blocks_;
    global_block_weights_ = other.global_block_weights_;
    strat_ = other.strat_;

    // Re-create the selector.
    // If selector has reference to internal, e.g., graph_,
    // that reference must be updated to *this.
    selector_factory_ = Factory<trace::SelectionStrategy, boost::shared_ptr<trace::Selector>>{};
    initialize_selector_factory();
    set(strat_);

    return *this;
}

bool TraceAnalyzer::insert_trace(const filesystem::path& path)
{
    std::cerr << "before parse_trace: " << (path / "tb-seq.bin").string() << std::endl;
    auto trace = parse_trace(path / "tb-seq.bin");

    if(compress_traces_)
    {
        assert(0 && "trace_compress should be disabled.\n");
        trace = compress(trace);
    }

    auto success = trace_graph_.insert(trace);

    if(success)
    {
        std::cerr << "successful parse_trace" << std::endl;
        assert(trace_selector_);

        trace_selector_->submit(trace);

        submit_to_unexecuted_block_registry(trace);
    }
    std::cerr << "after parse_trace" << std::endl;

    return success;
}

void TraceAnalyzer::insert_callback(std::function<void(Trace::ID)> cb)
{
    trace_graph_.insert_callback(cb);
}


boost::optional<Trace> TraceAnalyzer::next()
{
    assert(trace_selector_);

    return trace_selector_->next();
}

boost::optional<Trace> TraceAnalyzer::next_with_unexecuted_blocks()
{
    auto ret = optional<Trace>{};

    if(traces_with_unexecuted_blocks_.size() > 0)
    {
        auto it = traces_with_unexecuted_blocks_.begin();
        ret = optional<Trace>{*it};
        traces_with_unexecuted_blocks_.erase(it);
    }

    return ret;
}

void TraceAnalyzer::print_graph(bool only_branches, std::map<AddressRange, Entry>& elf_entries) const
{
    assert(trace_selector_);

    auto trace_scores = trace_selector_->trace_scores();

    trace_graph_.print_graph(only_branches, trace_scores, elf_entries);
}

size_t TraceAnalyzer::blocks_discovered_count() const
{
    return global_block_weights_.size();
}

void TraceAnalyzer::set_selection_strategy(const string& strat)
{
    using namespace trace;

    auto selection_strategy = SelectionStrategy{Random};

    if(strat == "bfs")
    {
        selection_strategy = BFS;
    }
    else if(strat == "exp-1")
    {
        selection_strategy = Exp1;
    }
    else if(strat == "exp-2")
    {
        selection_strategy = Exp2;
    }
    else if(strat == "random")
    {
        selection_strategy = Random;
    }
    else if(strat == "weighted")
    {
        selection_strategy = Weighted;
    }
    else if(strat == "fifo")
    {
        selection_strategy = FIFO;
    }
    else
    {
        throw std::runtime_error("invalid selection strategy: " + strat);
    }

    set(selection_strategy);
}

void TraceAnalyzer::submit_to_unexecuted_block_registry(const Trace& trace)
{
    if(contains_unexecuted(trace.get_blocks()))
    {
        traces_with_unexecuted_blocks_.insert(trace);
    }
}

void TraceAnalyzer::submit_executed(const Trace& trace)
{
    trace_graph_.submit_executed(trace);

    update_global_block_weights(trace);
    update_traces_with_unexecuted_blocks();
}

void TraceAnalyzer::update_global_block_weights(const Trace& trace)
{
    const auto& blocks = trace.get_blocks();
    for(const auto& b : blocks)
    {
        auto it = global_block_weights_.find(b);

        if(it == global_block_weights_.end())
        {
            global_block_weights_.insert(make_pair(b, 1));
        }
        else
        {
            it->second += 1; // Add 1 to weight.
        }
    }
}

void TraceAnalyzer::update_traces_with_unexecuted_blocks()
{
    auto& ts = traces_with_unexecuted_blocks_;
    auto to_erase = vector<Trace>{};

    for(const auto& t : ts)
    {
        if(!contains_unexecuted(t.get_blocks()))
        {
            to_erase.push_back(t);
        }
    }

    for(const auto& t : to_erase)
    {
        auto it = ts.find(t);

        assert(it != ts.end());

        ts.erase(it);
    }
}

bool TraceAnalyzer::contains_unexecuted(const Trace::Blocks& blocks)
{
    for(const auto& b : blocks)
    {
        auto it = global_block_weights_.find(b);

        if(it == global_block_weights_.end())
            return true;
        if(it->second == 0) // Under current impl. I don't think this is possible.
            return true;
    }

    return false;
}

void TraceAnalyzer::initialize_selector_factory()
{
    selector_factory_.insert(trace::BFS, [this]() {
        return boost::make_shared<trace::BreadthFirstSearchSelector>(this->trace_graph_);
    });
    selector_factory_.insert(trace::Random, []() {
        return boost::make_shared<trace::RandomSelector>();
    });
    selector_factory_.insert(trace::Weighted, []() {
        return boost::make_shared<trace::LeastTreadedSelector>();
    });
    selector_factory_.insert(trace::FIFO, []() {
        return boost::make_shared<trace::FIFOSelector>();
    });
    selector_factory_.insert(trace::Exp1, []() {
        return boost::make_shared<trace::WeightGroupSelector>();
    });
    selector_factory_.insert(trace::Exp2, [this]() {
        return boost::make_shared<trace::RecursiveDescentSelector>(this->trace_graph_);
    });   
}

void TraceAnalyzer::set(const trace::SelectionStrategy& strat)
{
    strat_ = strat;

    trace_selector_ = selector_factory_.create(strat);
}

auto TraceAnalyzer::compress_traces(bool b) -> void
{
    compress_traces_ = b;
}

void TraceAnalyzer::initialize_log()
{
    assert(0 && "pending implementation of crete::logger");
}

Trace parse_trace(const boost::filesystem::path& path)
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

}
