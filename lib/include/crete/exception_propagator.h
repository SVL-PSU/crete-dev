/***
 * Author: Christopher Havlicek
 */

#ifndef CRETE_EXCEPTION_PROPAGATOR_H
#define CRETE_EXCEPTION_PROPAGATOR_H

#include <exception>
#include <utility>

namespace crete
{

/**
 * @brief The ExceptionPropagator class
 *
 * For multithreading purposes.
 *
 * Execute any function with execute_and_catch()
 * and automatically catch the exception pointer.
 * Use is_exception_thrown() and rethrow_exception() to
 * propagate.
 *
 * May inherent from or use as a component.
 *
 * Remarks:
 *
 * Originally, execute_and_catch(Func f, Args&&... args) returned
 * decltype(f()). However, there is a problem. If an exception is thrown,
 * it is caught and the function returns - but what? Undefined behavior;
 * therefore, I've constrained f to only void returning functions
 * (the return value is ignored if not).
 *
 * TODO: Is a mutex necessary to protect eptr_? If so, it's a little complicated.
 *       std::mutex is not copyable nor movable, and that's problematic for how I use this class.
 *
 */
class ExceptionPropagator
{
public:
    template <typename Func, typename... Args>
    auto execute_and_catch(Func f, Args&&... args) -> void;
    auto is_exception_thrown() -> bool;
    [[noreturn]] auto rethrow_exception() -> void;

private:
    std::exception_ptr eptr_{nullptr};
};

template <typename Func, typename... Args>
auto ExceptionPropagator::execute_and_catch(Func f, Args&&... args) -> void
{
    try
    {
        f(std::forward<Args>(args)...);
    }
    catch(...)
    {
        eptr_ = std::current_exception();
    }
}

inline
auto ExceptionPropagator::is_exception_thrown() -> bool
{
    return static_cast<bool>(eptr_);
}

inline
auto ExceptionPropagator::rethrow_exception() -> void
{
    std::rethrow_exception(eptr_);
}

} // namespace crete

#endif // CRETE_EXCEPTION_PROPAGATOR_H
