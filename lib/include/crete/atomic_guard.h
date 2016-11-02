/***
 * Author: Christopher Havlicek
 ***/

#ifndef CRETE_ATOMIC_GUARD_H
#define CRETE_ATOMIC_GUARD_H

#include <utility>

#include <boost/thread/mutex.hpp>

namespace crete
{

/**
 * The intent of ScopeLock is two-fold.
 * 1. Lock and unlock the mutex according the present scope.
 * 2. Ensure access to the underlying object is hidden from direct access.
 *
 * To achieve (2), access is limited via operators. These include:
 *  - operator->
 *  - operator=
 *  - explicit conversion to underlying type T (static_cast<T>(), if/while/etc. if underlying type is bool).
 * No direct access to the object is allowed. For this reason, operator* is not defined.
 *
 * The strict limitation of (2) means that copying via copy-ctor or operator= possible only
 * when casted e.g., auto t = static_cast<T>(object_guard.aquire());
 *
 */
template <typename T>
class ScopeLock
{
public:
    ScopeLock(T& t, boost::mutex& m) : obj_{t}, mutex_{m}
    {
        mutex_.lock();
    }
    ~ScopeLock()
    {
        mutex_.unlock();
    }

    T* operator->()
    {
        return &obj_;
    }

    explicit operator T() { return obj_; }
    template <typename LHS>
    void operator=(const LHS& lhs)
    {
        obj_ = lhs;
    }

private:
    T& obj_;
    boost::mutex& mutex_;
};

/**
 * Thread-safe object wrapper. Allows for thread-safe exclusive (atomic) access to an object.
 *
 * Aquires the mutex when aquire() is called, and releases it when the returned object
 * (ScopeLock) goes out of scope.
 *
 * Usage:
 * AtomicGuard<Widget> widget;
 * // 1:
 * widget.aquire()->do_something();
 * // 2:
 * auto w = widget.aquire();
 * w->do_something();
 *
 * Remarks:
 * Race conditions can occur unless the following principle is kept.
 * No reference to any internal of Widget can outlive the scope lock.
 * This is particulary important for usage (2), but can also occur in usage (1), for example:
 * AtomicGuard<vector<shared_ptr<Widget>>> widgets; // Declared elsewhere.
 *
 * auto w = make_shared<Widget>();
 * w->do_setup();
 * widgets.acquire()->push_back(w);
 * w->do_something(); // Race condition here.
 *                    // 'w' has become an internal of widgets, but is used outside the locked.
 *                    // If another thread is accessing widgets, this is a race condition.
 * // Correction 1:
 * auto w = make_shared<Widget>();
 * w->do_setup();
 * w->do_something();
 * widgets.acquire()->push_back(w);
 * // Correction 2:
 * { auto lock = widgets.acquire();
 * auto w = make_shared<Widget>();
 * w->do_setup();
 * w->do_something();
 * lock->push_back(w) };
 */
template <typename T>
class AtomicGuard
{
public:
    template <typename... Args>
    AtomicGuard(Args&&... args) :
        obj_{std::forward<Args>(args)...} {}

    ScopeLock<T> acquire() { return ScopeLock<T>(obj_, mutex_); }

private:
    T obj_;
    boost::mutex mutex_;
};

} // namespace crete

#endif // CRETE_ATOMIC_GUARD_H
