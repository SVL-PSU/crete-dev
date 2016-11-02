#include <crete/selector.h>

#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/variate_generator.hpp>

#include <assert.h>
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <ctime>
#include <limits>
#include <iostream>

namespace crete
{
namespace trace
{

using Count = uint64_t;
using BlockWeights = std::unordered_map<Trace::Block, Count>;

auto weigh_blocks_by_frequency(const Trace::Blocks& blocks) -> BlockWeights
{
    auto weights = BlockWeights{};

    for(const auto& block : blocks)
    {
        auto it = weights.find(block);
        if(it == end(weights))
        {
            weights.insert({block, 1});
        }
        else
        {
            it->second += 1;
        }
    }

    return weights;
}

auto remove_nonduplicate(BlockWeights& weights) -> void
{
    for(auto it = begin(weights);
        it != end(weights);
        )
    {
        if(it->second < 2)
            weights.erase(it++);
        else
            ++it;
    }
}

auto calculate_weight_group_score(const Trace::Blocks& blocks) -> Score
{
    auto weights = weigh_blocks_by_frequency(blocks);

    // for each weight pass: pass(weights)

    remove_nonduplicate(weights);

    auto total_weight = std::accumulate(begin(weights),
                                        end(weights),
                                        Count{0},
                                        [](Count weight,
                                           const std::pair<Trace::Block, Count>& w)
                                        {
                                            return weight + w.second;
                                        });

    return Score{static_cast<Score>(total_weight)};
}

auto calculate_least_treaded_score(const Trace::Blocks& blocks) -> Score
{
    auto block_weights = weigh_blocks_by_frequency(blocks);

    auto sum = Count{0u};
    for(const auto& b : blocks)
    {
        auto it = block_weights.find(b);

        sum += it->second;
    }

    assert(blocks.size() > 0);

    return static_cast<Score>(sum) / blocks.size();
}

Selector::Selector() :
    random_engine_(std::time(0)) // Random seed based on execution time.
{

}

Selector::~Selector()
{

}

auto Selector::submit(const Trace& trace) -> void
{
    auto score = calculate_score(trace);

    auto success = trace_scores_.insert({trace, score}).second;

    assert(success);
}

void Selector::remove(const Trace& trace)
{
    trace_scores_.erase(trace);
}

auto Selector::next() -> const boost::optional<Trace>
{
    if(trace_scores_.size() == 0)
    {
        return boost::optional<Trace>{};
    }

    auto scores = std::multimap<Score, Trace>{};

    for(const auto& t : trace_scores_)
    {
        scores.insert({t.second, t.first});
    }

    const auto& lowest_score = begin(scores)->first;

    auto selection_range = scores.equal_range(lowest_score);

    auto index = generate_random(0,
                                 std::distance(selection_range.first,
                                               selection_range.second) - 1);

    std::advance(selection_range.first, index);

    assert(selection_range.first != selection_range.second);

    auto selected = selection_range.first->second;

    remove(selected);

    return boost::optional<Trace>{selected};
}

auto Selector::trace_scores() const -> Selector::TraceScoreMap
{
    return trace_scores_;
}

auto Selector::generate_random(std::size_t low, std::size_t high) -> std::size_t
{
    using namespace boost::random;

    uniform_int_distribution<std::size_t> dist(low,
                                               high);
    return dist(random_engine_);
}

auto WeightGroupSelector::calculate_score(const Trace& trace) -> Score
{
    auto score = calculate_weight_group_score(trace.get_blocks());

    return score;
}

RecursiveDescentSelector::RecursiveDescentSelector(TraceGraph& graph) :
    graph_(graph)
{

}

auto RecursiveDescentSelector::submit(const Trace& trace) -> void
{
    (void)trace;
}

auto RecursiveDescentSelector::next() -> const boost::optional<Trace>
{
    // This is calculating the score based on prefer-less-treaded.
    auto score_fn = [](const Trace::Blocks& blocks) {
         return calculate_least_treaded_score(blocks);
    };

    return boost::optional<Trace>{graph_.select_by_recursive_score(score_fn)};
}

auto RecursiveDescentSelector::calculate_score(const Trace& trace) -> Score
{
    (void)trace;

    return Score{0};
}

auto RecursiveDescentSelector::remove(const Trace& trace) -> void
{
    (void)trace;
}

BreadthFirstSearchSelector::BreadthFirstSearchSelector(TraceGraph& graph) :
    graph_(graph)
{
}

auto BreadthFirstSearchSelector::submit(const Trace& trace) -> void
{
    (void)trace;
}

auto BreadthFirstSearchSelector::next() -> const boost::optional<Trace>
{
    return graph_.next_bfs();
}

auto BreadthFirstSearchSelector::calculate_score(const Trace& trace) -> Score
{
    (void)trace;

    return Score{0};
}

auto BreadthFirstSearchSelector::remove(const Trace& trace) -> void
{
    (void)trace;
}

auto RandomSelector::calculate_score(const Trace& trace) -> Score
{
    (void)trace;

    auto score = Selector::generate_random(0,
                                           static_cast<size_t>(std::numeric_limits<double>::max()));

    return score;
}

auto LeastTreadedSelector::calculate_score(const Trace& trace) -> Score
{
    auto score = calculate_least_treaded_score(trace.get_blocks());

    return score;
}

void FIFOSelector::submit(const Trace& trace)
{
    trace_queue_.emplace_front(trace);
}

auto FIFOSelector::next() -> const boost::optional<Trace>
{
    if(trace_queue_.empty())
    {
        return boost::optional<Trace>{};
    }

    auto t = trace_queue_.back();

    trace_queue_.pop_back();

    return boost::optional<Trace>{t};
}

void FIFOSelector::remove(const Trace& trace)
{
    trace_queue_.erase(std::remove(trace_queue_.begin(),
                                   trace_queue_.end(),
                                   trace));
}

auto FIFOSelector::calculate_score(const Trace& trace) -> Score
{
    (void)trace;

    return Score{0};
}

} // namespace trace
} // namespace crete
