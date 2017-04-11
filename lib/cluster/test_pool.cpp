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

const static uint64_t BASE_TEST_CACHE_SIZE = 200;

bool TestPriority::operator() (const TestCase& lhs, const TestCase& rhs) const
{
  if (m_tc_sched_strat == FIFO)
  {
      return false;
  } else if (m_tc_sched_strat == BFS) {
      if(lhs.get_tt_last_node_index()
              > rhs.get_tt_last_node_index())
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
    : root_(root)
    ,tc_count_(0)
    ,next_(TestPriority(BFS))
    ,m_duplicated_tc_count(0) {}

auto TestPool::next() -> boost::optional<TestCase>
{
    boost::optional<TestCase> ret;
    assert(!ret);

    // XXX: Iterate until a non-duplicate test case is found
    while(!next_.empty() && !ret)
    {
        ret = get_complete_tc(next_.top());
        next_.pop();
    }

    return ret;
}

// The initial test case extracted from config will only be used to start the test,
// as it is open to change, such as invocation of "crete_make_concolic()" within
// the target exec under test
auto TestPool::insert_initial_tc_from_config(const TestCase& tc) -> bool
{
    assert(next_.empty());
    assert(tc_count_ == 0);

    next_.push(tc);
    return true;
}

auto TestPool::insert_initial_tcs(const std::vector<TestCase>& tcs) -> void
{
    assert(next_.empty());
    assert(tc_count_ == 0);

    for(const auto& tc : tcs)
    {
        insert_internal(tc);
    }
}

auto TestPool::insert(const std::vector<TestCase>& tcs) -> void
{
    // Special case for initial_tc_from_config
    if(tc_count_ == 0)
    {
        assert(!tcs.front().is_test_patch());
        write_test_case(tcs.front(), root_ / "test-case" / std::to_string(++tc_count_));
    }

    for(const auto& tc : tcs)
    {
        // patch tests are the newly generate tcs
        if(tc.is_test_patch())
        {
            tc.assert_tc_patch();
            insert_internal(tc);
        } else {
            insert_base_tc(tc);
            write_test_case(tc, root_ / "test-case-base-cache" / std::to_string(tc.get_issue_index()));
        }
    }
}

auto TestPool::count_all() const -> size_t
{
    return tc_count_;
}

auto TestPool::count_next() const -> size_t
{
    return next_.size();
}

auto TestPool::write_log(std::ostream& os) -> void
{
    os << "duplicated tc count from all_: " << m_duplicated_tc_count << endl;
}

auto TestPool::insert_internal(const TestCase& tc) -> bool
{
    next_.push(tc);

    write_test_case(tc, root_ / "test-case" / std::to_string(++tc_count_));

    return true;
}

auto TestPool::insert_base_tc(const TestCase& tc) -> BaseTestCache_ty::const_iterator
{
    if(base_tc_cache_.size() >= BASE_TEST_CACHE_SIZE)
    {
        base_tc_cache_.clear();
    }

    std::pair<BaseTestCache_ty::const_iterator, bool> it =
            base_tc_cache_.insert(std::make_pair(tc.get_issue_index(), tc));

    if(!it.second)
    {
        fprintf(stderr, "TestPool::insert_base_tc() error: duplicate issue_index in base_tc_cache_ (issue index = %lu),\n",
                it.first->first);

        write_test_case(it.first->second, root_ / "test-case-base-error" / "existing_based_tc.bin" );
        write_test_case(tc, root_ / "test-case-base-error" / "duplicate_base_tc.bin" );

        assert(0);
    }

    return it.first;
}

auto TestPool::get_base_tc(const TestCase& tc) -> BaseTestCache_ty::const_iterator
{
    assert(tc.is_test_patch());

    TestCaseIssueIndex tc_issue_index = tc.get_base_tc_issue_index();
    BaseTestCache_ty::const_iterator base_tc_it = base_tc_cache_.find(tc_issue_index);

    if(base_tc_it == base_tc_cache_.end())
    {
        fs::path base_tc_path = (root_ / "test-case-base-cache" / std::to_string(tc_issue_index));
        assert(fs::is_regular(base_tc_path));

        TestCase base_tc = retrieve_test_serialized(base_tc_path.string());
        assert(base_tc.get_issue_index() == tc_issue_index);

        base_tc_it = insert_base_tc(base_tc);
    }

    return base_tc_it;
}

auto TestPool::get_complete_tc(const TestCase& patch_tc) -> boost::optional<TestCase> const
{
    TestCase complete_tc;
    if(!patch_tc.is_test_patch())
    {
        complete_tc = patch_tc;
    } else {
        BaseTestCache_ty::const_iterator base_tc = get_base_tc(patch_tc);
        assert(base_tc != base_tc_cache_.end());

        complete_tc = generate_complete_tc_from_patch(patch_tc, base_tc->second);
    }

    // check whether the new complete_tc duplicates with issued tcs
    if(issued_tc_hash_pool_.insert(complete_tc.get_elements()).second)
    {
        complete_tc.set_issue_index(issued_tc_hash_pool_.size());
        return boost::optional<TestCase>{complete_tc};
    } else {
        ++m_duplicated_tc_count;

        return boost::optional<TestCase>();
    }
}

auto TestPool::write_test_case(const TestCase& tc, const fs::path out_path) -> void
{
    if(fs::exists(out_path))
    {
        fprintf(stderr, "out_path = %s\n", out_path.string().c_str());
        assert(0 && "[Test Pool] Duplicate test case name.\n");
    }

    if(!fs::exists(out_path.parent_path()))
        fs::create_directories(out_path.parent_path());

    fs::ofstream ofs(out_path,
                     std::ios_base::out | std::ios_base::binary);

    if(!ofs.good())
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{out_path.string()});
    }

    write_serialized(ofs, tc);
}

} // namespace cluster
} // namespace crete
