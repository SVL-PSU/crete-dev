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

TestPool::TestPool(const fs::path& root)
    : random_engine_{static_cast<unsigned>(std::time(0))}
    , root_{root}
{
}

auto TestPool::next() -> boost::optional<TestCase>
{
    if(next_.empty())
    {
        return boost::optional<TestCase>{};
    }

    // FIFO:
    auto tc = next_.back();
    next_.pop_back();

    return boost::optional<TestCase>{tc};

    // Random:
//    std::uniform_int_distribution<size_t> dist{0,
//                                               next_.size() - 1};

//    auto it = next_.begin();
//    std::advance(it,
//                 dist(random_engine_));
//    auto tc = boost::optional<TestCase>{*it};

//    next_.erase(it);

//    return tc;
}

auto TestPool::insert(const TestCase& tc) -> bool
{
    if(insert_tc_tree(tc))
    {
        next_.push_front(tc);
        return true;
    }

    return false;
}

auto TestPool::insert(const TestCase& tc, const TestCase& input_tc) -> bool
{
    if(insert_tc_tree(tc, input_tc))
    {
        next_.push_front(tc);
        return true;
    }

    return false;;
}

auto TestPool::insert(const std::vector<TestCase>& tcs) -> void
{
    for(const auto& tc : tcs)
    {
        insert(tc);
    }
}

auto TestPool::insert(const std::vector<TestCase>& new_tcs, const TestCase& input_tc) -> void
{
    for(const auto& tc : new_tcs)
    {
        insert(tc, input_tc);
    }
}

auto TestPool::clear() -> void
{
    next_.clear();
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
    for(boost::unordered_map<TestHash, TestCaseTreeNode>::const_iterator it = test_tree_.begin();
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
    std::string test_hash = to_test_hash(tc);
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
    std::string test_hash = to_test_hash(tc);
    if(test_tree_.find(test_hash) != test_tree_.end())
    {
        return false;
    }

    // Add node to tc_tree_ for this new tc
    test_tree_[test_hash] = TestCaseTreeNode();
    test_tree_[test_hash].m_tc_index = test_tree_.size();
    write_test_case(tc, test_tree_[test_hash].m_tc_index);

    // Set the parent tc_tree_node for this new
    std::string input_tc_hash = to_test_hash(input_tc);
    assert(test_tree_.find(input_tc_hash) != test_tree_.end());
    test_tree_[input_tc_hash].m_childern_tc_indexes.push_back(test_tree_[test_hash].m_tc_index);

    return true;
}

auto TestPool::to_test_hash(const TestCase& tc) -> TestHash
{
    std::stringstream ss;
    tc.write(ss);

    return ss.str();
}

} // namespace cluster
} // namespace crete
