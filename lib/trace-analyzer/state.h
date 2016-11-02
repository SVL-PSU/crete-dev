#ifndef CRETE_TRACE_STATE_H
#define CRETE_TRACE_STATE_H

namespace crete
{
namespace trace
{

using Block = uint64_t;
using Count = uint64_t;

// Should I define a state in terms of geographic location in the tree,
// or in the "state of choosing the next path"? I guess it depends on whether
// I'll have more than one state type.
// It would be conceptually easy to have the policy function map vertices in the tree from this vertex with a probability.
// I'm fairly confident this is the correct route. A single state type.
class State
{
public:
    State(const Block& block);

protected:
private:
   Count selected_count_; // How many times this state has been selected. (Same concept as weight?).
};

} // namespace trace
} // namespace crete

#endif // CRETE_TRACE_STATE_H

