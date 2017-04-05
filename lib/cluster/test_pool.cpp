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
    : root_{root}
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

    next_.push(tc);
    return true;
}

auto TestPool::insert_initial_tcs(const std::vector<TestCase>& tcs) -> void
{
    for(const auto& tc : tcs)
    {
        insert_internal(tc);
    }
}

auto TestPool::insert(const std::vector<TestCase>& tcs) -> void
{
    // Special case for initial_tc_from_config
    if(all_.empty())
    {
        assert(!tcs.front().is_test_patch());
        insert_to_all(tcs.front());
    }

    for(const auto& tc : tcs)
    {
        // patch tests are the newly generate tcs
        if(tc.is_test_patch())
        {
            tc.assert_tc_patch();
            insert_internal(tc);
        } else {
            std::pair<BaseTestCache_ty::const_iterator, bool> it =
                    base_tc_cache_.insert(std::make_pair(tc.complete_hash(), tc));
            if(!it.second){
                fprintf(stderr, "TestPool::insert() error: duplicate tc in base_tc_cache_ (complete hash = %s),\n",
                        it.first->first.c_str());

                write_test_case(it.first->second, root_ / "test-case-base-error" / "existing_based_tc.bin" );
                write_test_case(tc, root_ / "test-case-base-error" / "duplicate_base_tc.bin" );

                assert(0);
            }

            write_test_case(tc, root_ / "test-case-base-cache" / std::to_string(base_tc_cache_.size()));
        }
    }
}

auto TestPool::clear() -> void
{
    next_ = TestQueue(TestPriority(BFS));
    all_.clear();
    base_tc_cache_.clear();
}

auto TestPool::count_all() const -> size_t
{
    return all_.size();
}

auto TestPool::count_next() const -> size_t
{
    return next_.size();
}

auto TestPool::write_log(std::ostream& os) -> void
{
    os << "duplicated tc count from all_: " << m_duplicated_tc_count << endl;
}

auto TestPool::insert_to_all(const TestCase& tc) -> bool
{
    if(all_.insert(tc.complete_hash()).second)
    {
        write_test_case(tc, root_ / "test-case" / std::to_string(all_.size()));
        return true;
    } else {
        ++m_duplicated_tc_count;
    }

    return false;
}

auto TestPool::insert_internal(const TestCase& tc) -> bool
{
    if(insert_to_all(tc))
    {
        next_.push(tc);
        return true;
    }

    return false;
}

auto TestPool::get_complete_tc(const TestCase& patch_tc) -> boost::optional<TestCase> const
{
    if(!patch_tc.is_test_patch())
    {
        return boost::optional<TestCase>{patch_tc};
    }

    TestCaseHashComplete top_tc_base_hash = patch_tc.get_base_tc_hash();
    // xxx for now, all the tests in next_ are patches, whose base_hash should always be non-empty
    assert(!top_tc_base_hash.empty());

    BaseTestCache_ty::const_iterator base_tc = base_tc_cache_.find(top_tc_base_hash);
    assert(base_tc != base_tc_cache_.end());

    TestCase complete_tc = generate_complete_tc_from_patch(patch_tc, base_tc->second);

    // check whether the new complete_tc duplicates with cached complete tc
    if(base_tc_cache_.find(complete_tc.complete_hash()) == base_tc_cache_.end())
    {
        return boost::optional<TestCase>{complete_tc};
    } else {
#if 0
        fs::path out_temp("/tmp/debug_tp_next_" + std::to_string(patch_tc.hash()));
        fprintf(stderr, "duplicate test cases:\n"
                "tc_patch (%s) + base-tc (hash = %lu) duplicates with "
                "existing base tc (hash = %lu)\n",
                out_temp.string().c_str(), top_tc_base_hash, complete_tc.hash());

        write_test_case(patch_tc, out_temp);
        assert(0);
#endif
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
