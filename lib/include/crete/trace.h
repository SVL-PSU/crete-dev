#ifndef CRETE_TRACE_H
#define CRETE_TRACE_H

#include <vector>
#include <string>
#include <iterator>

#include <crete/util/cycle.h>

#include <boost/functional/hash/hash.hpp>

namespace crete
{
    class Trace
    {
    public:
        typedef std::string ID;
        typedef uint64_t Block;
        typedef std::vector<Block> Blocks;

    public:
        Trace(const ID& id, const Blocks& blocks) :
            id_(id),
            blocks_(blocks) {}

        size_t size() const { return blocks_.size(); }
        std::string get_id() const { return id_; }
        void set_id(const std::string& id) { id_ = id; }
        const Blocks& get_blocks() const { return blocks_; }
        Blocks& get_blocks() { return blocks_; }

    protected:
    private:
        std::string id_;
        Blocks blocks_;
    };

    inline
    bool operator<(const Trace& lhs, const Trace& rhs)
    {
        return lhs.get_id() < rhs.get_id();

//        const std::vector<std::size_t>& h1 = lhs.get_blocks();
//        const std::vector<std::size_t>& h2 = rhs.get_blocks();
//        return h1 < h2;
    }

    inline
    bool operator==(const Trace& lhs, const Trace& rhs)
    {
        return lhs.get_id() == rhs.get_id();
    }

    inline
    bool operator!=(const Trace& lhs, const Trace& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * @brief compress Replaces each cycle range with a single entry made from the combined
     * hash of the cycle range.
     * @param trace
     * @return Compressed trace.
     *
     * Remarks:
     * The first block is excluded from cycle detection. This is to avoid the corner case in which
     * the first block is involved in a cycle which may differ from trace to trace. Since the trace
     * graph assumes all traces share the first block in common, the first block cannot be replaced
     * by a hashed representation of the cycle it is involved in.
     *
     * 'trace' is not modified. Ordinarily, it would be const, but boost::hash_range doesn't
     * accept const iterators, so it was either take the trace as non-const, or use const_cast.
     * TODO: work around the const problem. Possibility: boost::iterator_adapter.
     */
    inline
    auto compress(Trace& trace) -> Trace
    {
        auto& blocks = trace.get_blocks();

        if(blocks.empty())
            return trace;

        auto ret = trace;
        auto& rblocks = ret.get_blocks();

        rblocks.clear();

        auto cs = find_cycles_custom(blocks.begin() + 1, blocks.end()); // See remarks in header comment for "begin() + 1" reasoning.

        std::sort(cs.begin(), cs.end());

        auto bit = blocks.begin();
        for(auto& range : cs)
        {
            std::copy(bit, range.first,
                      std::back_inserter(rblocks));

            auto hash = boost::hash_range(range.first, range.second);

            rblocks.emplace_back(hash);

            bit = range.second;
        }

        std::copy(bit, blocks.end(),
                  std::back_inserter(rblocks));

        return ret;
    }
}

#endif // CRETE_TRACE_H
