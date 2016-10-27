#ifndef CRETE_CYCLE_H
#define CRETE_CYCLE_H

#include <vector>
#include <deque>
#include <unordered_map>
#include <utility>
#include <type_traits>
#include <algorithm>
#include <cstdint>
#include <functional>

// Testing
#include <iostream>
#include <unordered_set>
#include <set>

namespace crete
{

template <typename InputIt>
struct CycleWalker
{
    CycleWalker(InputIt f, InputIt t) :
        from{f},
        current{f},
        to{t} {}
    InputIt from; // Where the CW starts from.
    InputIt current; // Current position of the CW.
    InputIt to; // Where the CW was generated.
};

template <typename InputIt>
auto find_cycles_custom(InputIt first, InputIt last) -> std::vector<std::pair<InputIt, InputIt>>;

/* Similar semantics to std::remove. Use in conjunction with T::erase().
 */
template <typename ForwardIt>
auto remove_subranges(ForwardIt first, ForwardIt last) -> ForwardIt
{
    for(auto i = first; i != last;)
    {
        last = std::remove_if(first, last, [i](const decltype(*first)& p)
        {
            return (i->first <= p.first && i->second > p.second) ||
                   (i->first < p.first && i->second >= p.second);
        });
        if(std::distance(first, i) > std::distance(first, last))
            i = last;
        else
            ++i;
    }

    return last;
}

/* Can be split into two cases: first is larger; second is larger;
 * After one is chosen, is the other cycle still valid?
 *
 * Ex:
 * {'x','a','b','c','a','b','c','d','e','b','c','d','e'};
 *       ^-------------------^
 *                       ^---------------------------^
 */
template <typename T>
auto remove_overlapping_ranges(T& ranges) -> void
{
    for(auto i = ranges.begin(); i != ranges.end();)
    {
        auto r = std::find_if(ranges.begin(), ranges.end(), [i](const decltype(*ranges.begin())& p)
        {
            return    i->first < p.first
                   && i->second < p.second
                   && i->second > p.first;
        });

        if(r == ranges.end())
        {
            ++i;
            continue;
        }

        if(std::distance(i->first, i->second) > std::distance(r->first, r->second))
        {
            // Check if r contains valid cycles after it's shortened.

            auto cs = find_cycles_custom(i->second, r->second);

            i = ranges.erase(r);

            if(!cs.empty())
            {
                for(const auto& e : cs)
                {
                    ranges.emplace_back(e);
                }

                i = ranges.begin();
            }
        }
        else
        {
            // Check if i contains valid cycles after it's shortened.

            auto cs = find_cycles_custom(i->first, r->first);

            i = ranges.erase(i);

            if(!cs.empty())
            {
                for(const auto& e : cs)
                {
                    ranges.emplace_back(e);
                }

                i = ranges.begin();
            }
        }
    }
}

/**
 * Uses pointer-walking and hash-based hybrid cycle detection. Written custom for CRETE.
 *
 * Remarks:
 * May miss cycles caught by unoptimized::find_cycles_custom; however, the total number of cycles
 * may be greater than the unoptimized version because the missed cycles may have been super-cycles
 * that would make sub-cycles redundant and would be removed.
 *
 * The primary difference v. unoptimized is that only a single CycleWalker originating from the same
 * position is allowed at any one time. This immensely reduces the number of CycleWalkers generated
 * in a given cycle.
 */
template <typename InputIt>
auto find_cycles_custom(InputIt first, InputIt last) -> std::vector<std::pair<InputIt, InputIt>>
{
    auto cs = std::vector<std::pair<InputIt, InputIt>>{};
    auto m = std::unordered_multimap<typename std::remove_reference<decltype(*first)>::type, InputIt>{};
    auto cws = std::vector<CycleWalker<InputIt>>{};
    auto it = first;

    struct ItHash
    {
        size_t operator()(InputIt i) const { return std::hash<decltype(&*i)>()(&*i); }
    };

    auto cws_set = std::unordered_set<InputIt, ItHash>{};

    while(it != last)
    {
        auto e = *it;

        for(auto jt = cws.begin(); jt != cws.end();)
        {
            ++jt->current;

            if(e != *jt->current)
            {
                if(*jt->current == *jt->from)
                {
                    auto f = jt->from;
                    while(*f == *jt->current && jt->current != last)
                    {
                        ++f;
                        ++jt->current;
                    }
                    if(jt->current >= jt->to)
                    {
                        cs.emplace_back(jt->from, jt->current);
                    }
                }

                jt = cws.erase(jt);
                cws_set.erase(jt->from);
            }
            else
            {
                ++jt;
            }
        }

        auto range = m.equal_range(e);
        for(auto jt = range.first; jt != range.second; ++jt)
        {
            if(cws_set.count(jt->second) == 0/* && std::distance(first, jt->second) * 2 < std::distance(first, last)*/)
            {
                cws.emplace_back(jt->second, it);
                cws_set.insert(jt->second);
            }
        }

        m.insert({e, it});

        ++it;
    }

    // Corner case where cycle ends at the last element.
    // Ex: abcabc. CW for 'a' will be at the first 'c' at this point.
    for(auto jt = cws.begin(); jt != cws.end(); ++jt)
    {
        auto n = std::next(jt->current);
        if(*n == *jt->from)
        {
            jt->current = n;
            auto f = jt->from;
            while(*f == *jt->current && jt->current != last)
            {
                ++f;
                ++jt->current;
            }
            if(jt->current >= jt->to)
            {
                cs.emplace_back(jt->from, jt->current);
            }
        }
    }

    std::sort(cs.begin(), cs.end());
    cs.erase(std::unique(cs.begin(), cs.end()),
             cs.end());

    cs.erase(remove_subranges(cs.begin(), cs.end()),
             cs.end());

    remove_overlapping_ranges(cs);

    return cs;
}

namespace unoptimized {

template <typename InputIt>
auto find_cycles_custom(InputIt first, InputIt last) -> std::vector<std::pair<InputIt, InputIt>>;

/* Can be split into two cases: first is larger; second is larger;
 * After one is chosen, is the other cycle still valid?
 *
 * Ex:
 * {'x','a','b','c','a','b','c','d','e','b','c','d','e'};
 *       ^-------------------^
 *                       ^---------------------------^
 */
template <typename T>
auto remove_overlapping_ranges(T& ranges) -> void
{
    for(auto i = ranges.begin(); i != ranges.end();)
    {
        auto r = std::find_if(ranges.begin(), ranges.end(), [i](const decltype(*ranges.begin())& p)
        {
            return    i->first < p.first
                   && i->second < p.second
                   && i->second > p.first;
        });

        if(r == ranges.end())
        {
            ++i;
            continue;
        }

        if(std::distance(i->first, i->second) > std::distance(r->first, r->second))
        {
            // Check if r contains valid cycles after it's shortened.

            auto cs = unoptimized::find_cycles_custom(i->second, r->second);

            i = ranges.erase(r);

            if(!cs.empty())
            {
                for(const auto& e : cs)
                {
                    ranges.emplace_back(e);
                }

                i = ranges.begin();
            }
        }
        else
        {
            // Check if i contains valid cycles after it's shortened.

            auto cs = unoptimized::find_cycles_custom(i->first, r->first);

            i = ranges.erase(i);

            if(!cs.empty())
            {
                for(const auto& e : cs)
                {
                    ranges.emplace_back(e);
                }

                i = ranges.begin();
            }
        }
    }
}

template <typename InputIt>
auto find_cycles_custom(InputIt first, InputIt last) -> std::vector<std::pair<InputIt, InputIt>>
{
    auto cs = std::vector<std::pair<InputIt, InputIt>>{};
    auto m = std::unordered_multimap<typename std::remove_reference<decltype(*first)>::type, InputIt>{};
    auto cws = std::vector<CycleWalker<InputIt>>{};
    auto it = first;

    while(it != last)
    {
        auto e = *it;

        for(auto jt = cws.begin(); jt != cws.end();)
        {
            ++jt->current;

            if(e != *jt->current)
            {
                if(*jt->current == *jt->from)
                {
                    auto f = jt->from;
                    while(*f == *jt->current && jt->current != last)
                    {
                        ++f;
                        ++jt->current;
                    }
                    if(jt->current >= jt->to)
                    {
                        cs.emplace_back(jt->from, jt->current);
                    }
                }

                jt = cws.erase(jt);
            }
            else
            {
                ++jt;
            }
        }

        auto range = m.equal_range(e);
        for(auto jt = range.first; jt != range.second; ++jt)
        {
            cws.emplace_back(jt->second, it);
        }

        m.insert({e, it});

        ++it;
    }

    // Corner case where cycle ends at the last element.
    // Ex: abcabc. CW for 'a' will be at the first 'c' at this point.
    for(auto jt = cws.begin(); jt != cws.end(); ++jt)
    {
        auto n = std::next(jt->current);
        if(*n == *jt->from)
        {
            jt->current = n;
            auto f = jt->from;
            while(*f == *jt->current && jt->current != last)
            {
                ++f;
                ++jt->current;
            }
            if(jt->current >= jt->to)
            {
                cs.emplace_back(jt->from, jt->current);
            }
        }
    }

    std::sort(cs.begin(), cs.end());
    cs.erase(std::unique(cs.begin(), cs.end()),
             cs.end());

    cs.erase(remove_subranges(cs.begin(), cs.end()),
             cs.end());

    unoptimized::remove_overlapping_ranges(cs);

    return cs;
}

} // unoptimized

/**
 * Brent's Algorithm. See Wikipedia.
 */
struct Cycle
{
    uint64_t offset;
    uint32_t length;
    uint32_t count;
};

template <typename InputIt>
auto find_cycle_brent(InputIt first, InputIt last) -> Cycle
{
    if (first == last)
        return {0, 0, 0};

    auto lambda = 1u;
    auto power = 1u;
    auto tortoise = first;
    auto hare = tortoise + 1;

    while (*tortoise != *hare)
    {
        if(hare == last)
            return {0, 0, 0};

        if (power == lambda)
        {
            tortoise = hare;
            power *= 2;
            lambda = 0;
        }

        ++hare;
        ++lambda;
    }

    auto mu = 0u;
    tortoise = hare = first;

    std::advance(hare, lambda);

    while (*tortoise != *hare)
    {
        ++tortoise;
        ++hare;
        ++mu;
    }

    // Non-Brent's alg. Determine how many times the cycle repeats.
    auto cycle = Cycle{mu, lambda, 1u};

    auto f = first + cycle.offset + cycle.length;
    auto g = first + cycle.offset;

    auto c = 0u;
    while(f != last && *f == *g)
    {
        if(c == cycle.length)
        {
            ++cycle.count;
            c = 0u;
        }

        ++f;
        ++g;
        ++c;
    }

    return cycle;
}

template <typename InputIt>
auto find_cycles_brent(InputIt first, InputIt last) -> std::vector<Cycle>
{
    auto cs = std::vector<Cycle>{};

    do
    {
        auto r = find_cycle(first, last);

        first += r.offset + (r.length * r.count);

        if(r.length == 0)
            first = last;
        else
            cs.emplace_back(r);

    }while(first != last);

    return cs;
}

} // namespace crete

#endif // CRETE_CYCLE_H
