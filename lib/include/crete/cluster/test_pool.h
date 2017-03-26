#ifndef CRETE_TEST_POOL_H_
#define CRETE_TEST_POOL_H_

#include <set>
#include <string>
#include <vector>
#include <queue>
#include <stdint.h>
#include <random>

#include <boost/filesystem/path.hpp>
#include <boost/optional.hpp>
#include <boost/unordered_map.hpp>

#include <crete/test_case.h>

namespace fs = boost::filesystem;

namespace crete
{
namespace cluster
{

struct TestCaseTreeNode
{
    uint64_t m_tc_index;
    std::vector<uint64_t> m_childern_tc_indexes;

    TestCaseTreeNode() {m_tc_index = -1;}
};

enum TestSchedStrat {FIFO, BFS};

class TestPriority
{
private:
    TestSchedStrat m_tc_sched_strat;

public:
    TestPriority(const TestSchedStrat& strat) {m_tc_sched_strat = strat;}
    bool operator() (const TestCase& lhs, const TestCase& rhs) const;
};

class TestPool
{
public:
    using TestQueue = std::priority_queue<TestCase, vector<TestCase>, TestPriority>;

private:
    fs::path root_;

    boost::unordered_map<TestCaseHash, TestCaseTreeNode> test_tree_;
    TestQueue next_;

public:
    TestPool(const fs::path& root);

    auto next() -> boost::optional<TestCase>;

    auto insert_initial_tc_from_config(const TestCase& tc) -> bool;
    auto insert_initial_tcs(const std::vector<TestCase>& tcs) -> void;

    auto insert(const std::vector<TestCase>& new_tcs, const TestCase& input_tc) -> void;

    auto clear() -> void;
    auto count_all() const -> size_t;
    auto count_next() const -> size_t;

    auto write_tc_tree(std::ostream& os) -> void const;

private:
    auto write_test_case(const TestCase& tc, const uint64_t tc_index) -> void;
    auto insert_tc_tree(const TestCase& tc) -> bool;
    auto insert_tc_tree(const TestCase& tc, const TestCase& input_tc) -> bool;

    auto insert_internal(const TestCase& tc, const TestCase& input_tc) -> bool;
    auto insert_initial_tc(const TestCase& tc) -> bool;

    auto to_test_hash(const TestCase& tc) -> TestCaseHash;
};

} // namespace cluster
} // namespace crete

#endif // CRETE_TEST_POOL_H_
