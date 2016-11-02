#ifndef CRETE_SELECTOR_H
#define CRETE_SELECTOR_H

#include <boost/optional.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/shared_ptr.hpp>

#include <set>
#include <map>

#include <crete/trace.h>
#include <crete/trace_graph.h>

namespace crete
{
namespace trace
{

const char* const selection_strategy_strings[] = {"bfs",
                                                  "exp1",
                                                  "exp2",
                                                  "random",
                                                  "weighted",
                                                  "fifo"};
enum SelectionStrategy { BFS, Exp1, Exp2, Random, Weighted, FIFO };

using Score = double;

//--------- Free Functions ---------
auto calculate_weight_group_score(const Trace::Blocks& blocks/*,
                                  const BlockPass& pass*/) -> Score;
auto calculate_least_treaded_score(const Trace::Blocks& blocks/*,
                                  const BlockPass& pass*/) -> Score;

//--------- Classes ---------
class Selector
{
public:
    using TraceScoreMap = std::map<Trace, Score>;

public:
    Selector();
    virtual ~Selector();

    virtual auto submit(const Trace& trace) -> void;
    virtual auto next() -> const boost::optional<Trace>;
    virtual auto remove(const Trace& trace) -> void;

//    auto include(const BlockPass& pass) -> void;

    virtual auto trace_scores() const -> TraceScoreMap;

protected:
    virtual auto calculate_score(const Trace& trace/*,
                                 const BlockPass& pass*/) -> Score = 0;

    auto generate_random(std::size_t low,
                         std::size_t high) -> std::size_t;

private:
   TraceScoreMap trace_scores_;
   boost::random::mt19937 random_engine_;
};

class WeightGroupSelector final : public Selector
{
public:
protected:
   auto calculate_score(const Trace& trace) -> Score override;

private:
};

class RecursiveDescentSelector final : public Selector
{
public:
    RecursiveDescentSelector(TraceGraph& graph);

    auto submit(const Trace& trace) -> void override;
    auto next() -> const boost::optional<Trace> override;
    auto remove(const Trace& trace) -> void override;

protected:
    auto calculate_score(const Trace& trace) -> Score override;

private:
    TraceGraph& graph_;
};

class BreadthFirstSearchSelector final : public Selector
{
public:
    BreadthFirstSearchSelector(TraceGraph& graph);

    auto submit(const Trace& trace) -> void override;
    auto next() -> const boost::optional<Trace> override;
    auto remove(const Trace& trace) -> void override;

protected:
    auto calculate_score(const Trace& trace) -> Score override;

private:
    TraceGraph& graph_;
};

class RandomSelector final : public Selector
{
public:

protected:
    auto calculate_score(const Trace& trace) -> Score override;

private:
};

class LeastTreadedSelector final : public Selector
{
public:

protected:
    auto calculate_score(const Trace& trace) -> Score override;

private:
};

class FIFOSelector final : public Selector
{
public:
    auto submit(const Trace& trace) -> void override;
    auto next() -> const boost::optional<Trace> override;
    auto remove(const Trace& trace) -> void override;

protected:
    auto calculate_score(const Trace& trace) -> Score override;

private:
    std::deque<Trace> trace_queue_;
};

} // namespace trace
} // namespace crete

#endif // CRETE_SELECTOR_H
