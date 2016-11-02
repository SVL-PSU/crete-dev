#ifndef CRETE_TEST_POOL_H_
#define CRETE_TEST_POOL_H_

#include <set>
#include <string>
#include <vector>
#include <deque>
#include <stdint.h>
#include <random>

#include <boost/filesystem/path.hpp>
#include <boost/optional.hpp>
#include <boost/unordered_map.hpp>

#include <crete/test_case.h>

namespace crete
{
namespace cluster
{

struct TestCaseTreeNode
{
    uint64_t m_tc_index;
    std::vector<uint64_t> m_childern_tc_indexes;

    TestCaseTreeNode() {};
};

class TestPool
{
public:
    using TracePath = boost::filesystem::path;
    using TestQueue = std::deque<crete::TestCase>;
    using TestHash = std::string;

private:
    boost::unordered_map<TestHash, TestCaseTreeNode> test_tree_;

    TestQueue next_;
    std::mt19937 random_engine_; // TODO: currently unused in favor of FIFO; however, should random be optional?
    boost::filesystem::path root_;

public:
    TestPool(const boost::filesystem::path& root);

    auto next() -> boost::optional<TestCase>;

    auto insert(const TestCase& tc) -> bool;
    auto insert(const TestCase& tc, const TestCase& input_tc) -> bool;

    auto insert(const std::vector<TestCase>& tcs) -> void;
    auto insert(const std::vector<TestCase>& new_tcs, const TestCase& input_tc) -> void;

    auto clear() -> void;
    auto count_all() const -> size_t;
    auto count_next() const -> size_t;

    auto write_tc_tree(std::ostream& os) -> void const;

private:
    auto write_test_case(const TestCase& tc, const uint64_t tc_index) -> void;
    auto insert_tc_tree(const TestCase& tc) -> bool;
    auto insert_tc_tree(const TestCase& tc, const TestCase& input_tc) -> bool;

    auto to_test_hash(const TestCase& tc) -> TestHash;
};

} // namespace cluster
} // namespace crete

#endif // CRETE_TEST_POOL_H_
