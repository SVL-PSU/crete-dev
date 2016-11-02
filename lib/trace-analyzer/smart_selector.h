#ifndef CRETE_SMART_SELECTOR_H
#define CRETE_SMART_SELECTOR_H

#include "selector.h"

namespace crete
{
namespace trace
{

// I don't think this is the agent. I think this is the apparatus in which agent, states, environment live.
class SmartSelector : public Selector
{
public:
    using Probability = double;
    using TransitionTable = std::vector<std::vector<Probability>>;

public:
    SmartSelector();

    auto submit(const Trace& trace) -> void;
    auto next() -> const boost::optional<Trace>;
    auto remove(const Trace& trace) -> void;
    auto trace_scores() const -> TraceScoreMap;

protected:
    auto calculate_score(const Trace& trace) -> Score;

private:
    TransitionTable transition_table_;
};

} // namespace trace
} // namespace crete

#endif // CRETE_SMART_SELECTOR_H

