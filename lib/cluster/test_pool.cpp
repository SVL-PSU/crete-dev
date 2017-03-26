#include <crete/cluster/test_pool.h>

#include <iostream>
#include <cstdlib>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <crete/exception.h>

namespace fs = boost::filesystem;

namespace crete
{
namespace cluster
{

bool TestPriority::operator() (const TestCase& lhs, const TestCase& rhs) const
{
  if (m_tc_sched_strat == FIFO)
  {
      return false;
  } else if (m_tc_sched_strat == BFS) {
      if(lhs.get_traceTag_explored_nodes().size()
              > rhs.get_traceTag_explored_nodes().size())
      {
          return true;
      } else {
          return false;
      }
  } else  {
      fprintf(stderr, "[CRETE ERROR] un-recognized test priority: \'%d\'\n", m_tc_sched_strat);
      assert(0);
      return false;
  }
}

TestPool::TestPool(const fs::path& root)
    : root_{root}
    ,next_(TestPriority(BFS)) {}

auto TestPool::next() -> boost::optional<TestCase>
{
    if(next_.empty())
    {
        return boost::optional<TestCase>{};
    }

    auto tc = next_.top();
    next_.pop();

    return boost::optional<TestCase>{tc};
}

// The initial test case extracted from config will only be used to start the test,
// as it is open to change, such as invocation of "crete_make_concolic()" within
// the target exec under test
auto TestPool::insert_initial_tc_from_config(const TestCase& tc) -> bool
{
    assert(test_tree_.empty());
    assert(next_.empty());

    next_.push(tc);
    return true;
}

auto TestPool::insert_initial_tc(const TestCase& tc) -> bool
{
    if(insert_tc_tree(tc))
    {
        next_.push(tc);
        return true;
    }

    return false;
}

auto TestPool::insert_internal(const TestCase& tc, const TestCase& input_tc) -> bool
{
    if(insert_tc_tree(tc, input_tc))
    {
        next_.push(tc);
        return true;
    }

    return false;;
}

auto TestPool::insert_initial_tcs(const std::vector<TestCase>& tcs) -> void
{
    for(const auto& tc : tcs)
    {
        insert_initial_tc(tc);
    }
}

auto TestPool::insert(const std::vector<TestCase>& new_tcs, const TestCase& input_tc) -> void
{
    if(test_tree_.empty())
    {
        insert_tc_tree(input_tc);
    }

    for(const auto& tc : new_tcs)
    {
        insert_internal(tc, input_tc);
    }
}

auto TestPool::clear() -> void
{
    next_ = TestQueue(TestPriority(BFS));
    test_tree_.clear();
}

auto TestPool::count_all() const -> size_t
{
    return test_tree_.size();
}

auto TestPool::count_next() const -> size_t
{
    return next_.size();
}

auto TestPool::write_tc_tree(std::ostream& os) -> void const
{
    for(boost::unordered_map<TestCaseHash, TestCaseTreeNode>::const_iterator it = test_tree_.begin();
            it != test_tree_.end(); ++it) {
        os << "Node tc-" << it->second.m_tc_index << ": [";
        for(std::vector<uint64_t>::const_iterator c_it = it->second.m_childern_tc_indexes.begin();
                c_it != it->second.m_childern_tc_indexes.end(); ++c_it) {
            os << *c_it << " ";
        }
        os << "]\n";
    }
}

auto TestPool::write_test_case(const TestCase& tc, const uint64_t tc_index) -> void
{
    namespace fs = boost::filesystem;

    auto tc_root = root_ / "test-case";

    if(!fs::exists(tc_root))
        fs::create_directories(tc_root);

    auto path = tc_root / std::to_string(tc_index);

    assert(!fs::exists(path) && "[Test Pool] Duplicate test case name.\n");
    fs::ofstream ofs(path,
                     std::ios_base::out | std::ios_base::binary);

    if(!ofs.good())
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{path.string()});
    }

    tc.write(ofs);
}

auto TestPool::insert_tc_tree(const TestCase& tc) -> bool
{
    TestCaseHash test_hash = to_test_hash(tc);
    if(test_tree_.find(test_hash) != test_tree_.end())
    {
        return false;
    }

    // Add node to tc_tree_ for this new tc
    test_tree_[test_hash] = TestCaseTreeNode();
    test_tree_[test_hash].m_tc_index = test_tree_.size();
    write_test_case(tc, test_tree_[test_hash].m_tc_index);

    return true;
}

auto TestPool::insert_tc_tree(const TestCase& tc, const TestCase& input_tc) -> bool
{
    TestCaseHash test_hash = to_test_hash(tc);
    if(test_tree_.find(test_hash) != test_tree_.end())
    {
        return false;
    }

    // Add node to tc_tree_ for this new tc
    test_tree_[test_hash] = TestCaseTreeNode();
    test_tree_[test_hash].m_tc_index = test_tree_.size();
    write_test_case(tc, test_tree_[test_hash].m_tc_index);

    // Set the parent tc_tree_node for this new
    TestCaseHash input_tc_hash = to_test_hash(input_tc);
    assert(test_tree_.find(input_tc_hash) != test_tree_.end());
    test_tree_[input_tc_hash].m_childern_tc_indexes.push_back(test_tree_[test_hash].m_tc_index);

    return true;
}

auto TestPool::to_test_hash(const TestCase& tc) -> TestCaseHash
{
    return tc.hash();
}

} // namespace cluster
} // namespace crete
