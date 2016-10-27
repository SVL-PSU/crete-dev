#ifndef CRETE_PROCESS_H
#define CRETE_PROCESS_H

#include <boost/process/status.hpp>
#include <boost/lexical_cast.hpp>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <crete/exception.h>

namespace crete
{

namespace process
{

/**
 * @brief is_running
 * @param pid
 * @return
 *
 * Remarks:
 * For unknown reasons, waitpid() doesn't work in all use cases (e.g., developer mode).
 * Similarly, manually checking for the existence of /proc/<pid> only works
 * if the process result has been claimed; therefore, both are used together.
 *
 * Warning:
 * Due to the particular implemenation using waitpid(), which "reaps" an exited proccess,
 * further calls to waitpid (e.g., boost::process::child::wait()) will fail.
 */
inline
bool is_running(pid_t pid)
{
    int status;
//    return waitpid(pid, &status, WNOHANG) == 0;
    waitpid(pid, &status, WNOHANG);

    (void)status;

    struct stat s;

    return stat(("/proc/" + boost::lexical_cast<std::string>(pid)).c_str(), &s) != -1;
}

/**
 * @brief is_exit_status_zero
 * @param s
 * @return true/false
 *
 * Remarks:
 * According to the current impl. of status::exit_status(), assert(exited()) is checked.
 * I assume there is a good reason, but the effect is that whenever a process does not
 * 'exit gracefully,' when exit_status() is called, the assertion is raised;
 * therefore, I provide this utility that short-circuit returns false if exited() is false.
 *
 * Prefer calling this to checking manually.
 */
inline
bool is_exit_status_zero(const boost::process::status& s)
{
    return
            s.exited() && s.exit_status() == 0;
}

} // namespace process
} // namespace crete

#endif // CRETE_PROCESS_H
