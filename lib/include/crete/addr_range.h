#ifndef CRETE_ADDR_RANGE_H
#define CRETE_ADDR_RANGE_H

#include <stdint.h>

namespace crete
{

class AddressRange
{
public:
    AddressRange(uint64_t begin, uint64_t end) : begin_(begin), end_(end) {}

    uint64_t begin() const { return begin_; }
    uint64_t end() const { return end_; }

private:
    uint64_t begin_, end_;
};

inline bool is_in(const AddressRange& range, uint64_t addr)
{
    return
            range.begin() <= addr &&
            range.end() > addr;
}
inline bool operator==(const AddressRange& rhs, const AddressRange& lhs)
{
    return
            rhs.begin() == lhs.begin() &&
            rhs.end() == lhs.end();
}

inline bool operator<(const AddressRange& rhs, const AddressRange& lhs)
{
    return
            rhs.begin() < lhs.begin();
}

} // namespace crete

#endif // CRETE_ADDR_RANGE_H
