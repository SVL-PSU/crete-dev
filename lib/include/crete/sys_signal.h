#ifndef CRETE_SYS_SIGNAL_H
#define CRETE_SYS_SIGNAL_H

#include <crete/stacktrace.h>

#include <unistd.h>
#include <signal.h>

namespace crete
{
namespace sys
{

inline
void signal_handler(int signum)
{
    switch(signum)
    {
    case SIGSEGV:
        print_stacktrace();
        break;
    default:
        assert(0 && "[CRETE] unhandled signal");
    }
}

inline 
void register_default_handlers()
{
    signal(SIGSEGV, signal_handler);
}

} // namespace sys
} // namespace crete

#endif // CRETE_SYS_SIGNAL_H
