#include <boost/test/unit_test.hpp>

#include <stdlib.h>
#include <fcntl.h>

#include "crete/harness.h"

const char* const posix_file_symbolic_1 = "symbolic.1";
const char* const posix_file_concrete_1 = "concrete.1";

const size_t max_sym_files = 32; // MAX_FDS == 32, so it makes sense.


struct PosixGlobalContext
{
    PosixGlobalContext()
    {
        crete_initialize_concolic_posix_files(max_sym_files);

        crete_make_concolic_file_posix(posix_file_symbolic_1, 32);
    }
    ~PosixGlobalContext()
    {
        // TODO: cleanup posix_files? I know I do a malloc...
    }
};

BOOST_GLOBAL_FIXTURE(PosixGlobalContext);

BOOST_AUTO_TEST_SUITE(crete_hook_posix)

BOOST_AUTO_TEST_CASE(open_close_valid_return_values)
{
    int fdc = open(posix_file_concrete_1,
                   O_CREAT | O_RDWR,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    int fds = open(posix_file_symbolic_1,
                   O_CREAT | O_RDWR,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    BOOST_CHECK_GE(fdc, 0);
    BOOST_CHECK_GE(fds, 0);

    BOOST_CHECK_EQUAL(crete_posix_is_symbolic_fd(fdc), 0);
    BOOST_CHECK_EQUAL(crete_posix_is_symbolic_fd(fds), 1);

    BOOST_CHECK_EQUAL(close(fdc), 0);
    BOOST_CHECK_EQUAL(close(fds), 0);
}


BOOST_AUTO_TEST_CASE(read_correct_result)
{

    int fdc = open(posix_file_concrete_1,
                   O_CREAT | O_RDWR,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    int fds = open(posix_file_symbolic_1,
                   O_CREAT | O_RDWR,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    BOOST_CHECK_GE(fdc, 0);
    BOOST_CHECK_GE(fds, 0);

    char buf[32];
    BOOST_CHECK_EQUAL(read(fds, (void*)buf, sizeof(buf)), sizeof(buf));

//    BOOST_CHECK_EQUAL(memcmp(buf, ));

    BOOST_CHECK_EQUAL(close(fdc), 0);
    BOOST_CHECK_EQUAL(close(fds), 0);
}

BOOST_AUTO_TEST_SUITE_END()
