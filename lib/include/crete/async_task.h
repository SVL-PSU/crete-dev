#ifndef CRETE_ASYNC_TASK_H
#define CRETE_ASYNC_TASK_H

#include <boost/thread.hpp>
#include <atomic>
#include <iostream>
#include <mutex>

namespace crete
{

/**
 * @brief The AsyncTask class
 *
 * Remarks:
 *
 * There seems to be considerable overlap with this and std::packaged_task.
 *
 * Default constructed instances return true for is_finished. This is controversial,
 * but a convenience for how CRETE uses the class. One could argue that calls to is_finished
 * from a default constructed state should be undefined (or raise an exception), given that without a task
 * to execute, the question of whether the task is finished or not is absurd. An alternative
 * may be to add an additional function, has_task, that can be queried in addition to is_finished,
 * as in: if( has_task() && is_finished ) ....
 */
class AsyncTask
{
public:
    AsyncTask();
    template <typename Func, typename... Args>
    AsyncTask(Func f, Args&&... args);
    ~AsyncTask();

    auto operator=(AsyncTask&& other) -> AsyncTask&;

    auto is_finished() const -> bool;
    auto is_exception_thrown() const -> bool;
    [[noreturn]] auto rethrow_exception() -> void;
    auto release_exception() -> std::exception_ptr;

private:
    boost::thread thread_;
    std::atomic<bool> finished_flag_{false};
    std::exception_ptr eptr_;
    mutable std::mutex eptr_mutex_;
};

template <typename Func, typename... Args>
AsyncTask::AsyncTask(Func f,
                     Args&&... args)
{
    auto wrapper = [this, args...](Func f) { // TODO: ideally, this should do perfect forwarding to the wrapper.

        try // Need separate try...catch b/c this is a separate thread.
        {
            f(args...);
        }
        catch(...)
        {
            eptr_mutex_.lock();

            eptr_ = std::current_exception();

            eptr_mutex_.unlock();
        }

        finished_flag_.exchange(true, std::memory_order_seq_cst);
    };

    thread_ = boost::thread{wrapper, f};
}

inline
AsyncTask::AsyncTask()
{
    // This is the default ctor, so we do not have a task, and therefore are 'finished.'
    finished_flag_.exchange(true, std::memory_order_seq_cst);
}

inline
AsyncTask::~AsyncTask()
{
    if(thread_.joinable())
    {
        if(is_exception_thrown())
        {
            try
            {
                std::rethrow_exception(eptr_);
            }
            catch(std::exception& e)
            {
                auto except_info = boost::diagnostic_information(e);
                std::cerr << "==================================\n"
                        "Exception from AsyncTask:\n";
                std::cerr << except_info << std::endl;
                std::cerr << "==================================\n";

                assert(0);
            }
        }

        thread_.join();
    }
}

inline
auto AsyncTask::is_finished() const -> bool
{
    return finished_flag_.load(std::memory_order_seq_cst);
}

inline
auto AsyncTask::is_exception_thrown() const -> bool
{
    std::lock_guard<std::mutex> lock{eptr_mutex_};

    return static_cast<bool>(eptr_);
}

inline
auto AsyncTask::rethrow_exception() -> void
{
    std::lock_guard<std::mutex> lock{eptr_mutex_};

    auto tmp = eptr_;

    eptr_ = std::exception_ptr{};

    std::rethrow_exception(tmp);
}

inline
auto AsyncTask::release_exception() -> std::exception_ptr
{
    std::lock_guard<std::mutex> lock{eptr_mutex_};

    auto tmp = eptr_;
    eptr_ = std::exception_ptr{};

    return tmp;
}

} // namespace crete

#endif // CRETE_ASYNC_TASK_H
