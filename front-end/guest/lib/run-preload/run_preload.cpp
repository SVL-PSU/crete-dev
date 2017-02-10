#include <crete/common.h>
#include <crete/harness.h>
#include <crete/custom_instr.h>
#include <crete/harness_config.h>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp> // Needed for text_iarchive (for some reason).
#include <boost/property_tree/xml_parser.hpp>
#include <boost/filesystem.hpp>

#include <dlfcn.h>
#include <cassert>
#include <cstdlib>

#include <iostream>
#include <string>
#include <stdexcept>
#include <stdio.h>

#if CRETE_HOST_ENV
#include "run_config.h"
#endif // CRETE_HOST_ENV

using namespace std;
using namespace crete;
namespace fs = boost::filesystem;

void print_back_trace();

config::HarnessConfiguration crete_load_configuration()
{
    std::ifstream ifs(CRETE_CONFIG_SERIALIZED_PATH,
                      ios_base::in | ios_base::binary);

    if(!ifs.good())
    {
        throw std::runtime_error("failed to open file: " + std::string(CRETE_CONFIG_SERIALIZED_PATH));
    }

    boost::archive::text_iarchive ia(ifs);
    config::HarnessConfiguration config;
    ia >> config;

    return config;
}

// Support conoclic/concrete stdin operated by standard c functions (eg. fgets, fread, scanf, etc)
static void crete_process_stdin_libc(const config::STDStream stdin_config)
{
    uint64_t size = stdin_config.size;

    //1. Allocate buffer "crete_stdin_buffer" for stdin_ramdisk and write conoclic/concrete value to it
    char* crete_stdin_buffer = (char *)malloc(size);
    assert(crete_stdin_buffer &&
            "malloc() failed in preload::crete_make_concolic_stdin_std().\n");
    memset(crete_stdin_buffer, 0, size);

    if(stdin_config.concolic)
    {
        crete_make_concolic(crete_stdin_buffer, size, "crete-stdin");
    } else {
        size_t length = stdin_config.value.copy(crete_stdin_buffer, size);
        assert(length == size);
    }

    //2. Write concolic/concrete value from buffer "crete_stdin_buffer" to file "crete_stdin_ramdisk"
    char stdin_ramdisk_file[512];
    memset(stdin_ramdisk_file, 0, 512);
    sprintf(stdin_ramdisk_file, "%s/crete_stdin_ramdisk", CRETE_RAMDISK_PATH);

    //TODO: xxx shall I open it with flag "b" (binary)?
    FILE *crete_stdin_fd = fopen (stdin_ramdisk_file, "wb");
    if(crete_stdin_fd == NULL) {
      printf("Error: can't open file %s for writing\n", stdin_ramdisk_file);
      exit(-1);
    }

    size_t out_result = fwrite(crete_stdin_buffer, 1, size, crete_stdin_fd);

    if(out_result != size) {
      printf("Error: write symbolic content to %s failed\n", stdin_ramdisk_file);
      exit(-1);
    }

    fclose(crete_stdin_fd);

    //3. Redirect stdin to file "crete_stdin_ramdisk"
    //TODO: xxx shall I open it with flag "b" (binary)?
    if(freopen(stdin_ramdisk_file, "rb", stdin) == NULL) {
      printf("Error: Redirect stdin to %s by freopen() failed.\n",
              stdin_ramdisk_file);
    }
}

static void crete_process_stdin_posix(const config::STDStream stdin_config)
{
    // TODO: xxx how about non-concolic (concrete) stdin for posix
    if(stdin_config.concolic)
    {
        uint64_t size = stdin_config.size;
        char* crete_stdin_buffer = (char *)malloc(size);
        assert(crete_stdin_buffer &&
                "malloc() failed in preload::crete_make_concolic_stdin_posix_blank().\n");
        memset(crete_stdin_buffer, 0, size);
        crete_make_concolic(crete_stdin_buffer, size, "crete-stdin-posix");
    }
}

static void crete_process_stdin(const config::HarnessConfiguration& hconfig)
{
    const config::STDStream stdin_config = hconfig.get_stdin();

    if(stdin_config.size > 0)
    {
        crete_process_stdin_libc(stdin_config);
        crete_process_stdin_posix(stdin_config);
    }
}

// replace the original crete_make_concolic_file_std() for supporting libc file operations
void crete_make_concolic_file_libc_std(const config::File& file)
{
    string filename = file.path.filename().string();

    assert(!filename.empty() && "[CRETE] file name must be valid");
    assert(file.size > 0 && "[CRETE] file size must be greater than zero");

    char* buffer = new char [file.size];
    memset(buffer, 0, file.size);

    crete_make_concolic(buffer, file.size, filename.c_str());

    // write symbolic value to ramdisk file
    string file_full_path = file.path.string();
    FILE *out_fd = fopen (file_full_path.c_str(), "wb");
    if(out_fd == NULL) {
      printf("Error: can't open file %s for writing\n", file_full_path.c_str());
      throw runtime_error("failed to open file in preload for making concolic file\n");
    }

    size_t out_result = fwrite(buffer, 1, file.size, out_fd);

    if(out_result != file.size) {
      throw runtime_error("wrong size of writing symbolic values in preload for making concolic file\n");
    }

    fclose(out_fd);
}

void crete_make_concolic_file_posix_blank(const config::File& file)
{
    string filename = file.path.filename().string();

    assert(!filename.empty() && "[CRETE] file name must be valid");
    assert(file.size > 0 && "[CRETE] file size must be greater than zero");

    filename = filename + "-posix";

    char* buffer = new char [file.size];
    memset(buffer, 0, file.size);

    string file_full_path = file.path.string();
    crete_make_concolic(buffer, file.size, filename.c_str());

    memset(buffer, 0, file.size);
}

void crete_make_concolic_file(const config::File& file)
{
    // Since we don't know what file routines will be used (e.g., open() vs fopen()),
    // initialize both kinds.
    crete_make_concolic_file_libc_std(file);
    crete_make_concolic_file_posix_blank(file);
}

void crete_process_files(const config::HarnessConfiguration& hconfig)
{
    const config::Files& files = hconfig.get_files();
    const size_t file_count = files.size();

    if(file_count == 0)
        return;

    for(config::Files::const_iterator it = files.begin();
        it != files.end();
        ++it)
    {
        if(!it->concolic)
            continue;

        crete_make_concolic_file(*it);
    }
}

static void crete_process_args(const config::Arguments& guest_config_args,
        int argc, char**& argv)
{
    assert(guest_config_args.size() == (argc - 1));

    for(config::Arguments::const_iterator it = guest_config_args.begin();
            it != guest_config_args.end(); ++it) {
        if(it->concolic)
        {
            assert(it->index < argc);

            stringstream concolic_name;
            concolic_name << "argv_" << it->index;

            // TODO: xxx memory leak here, but who care?
            char *buffer = new char [it->size + 1];
            memset(buffer, 0, it->size + 1);
            crete_make_concolic(buffer, it->size, concolic_name.str().c_str());
            argv[it->index] = buffer;
        } else {
            // sanity check
            // Note: string::compare() will not work, as it->value
            //       can contain several '\0' at the end while argv doesn't
            assert(strcmp(it->value.c_str(), argv[it->index]) == 0 &&
                    "[run-preload] Sanity check on concrete args failed\n");
        }
    }
}

void crete_process_configuration(const config::HarnessConfiguration& hconfig,
                                 int argc, char**& argv)
{
    // Note: order matters.
    crete_process_args(hconfig.get_arguments(),argc, argv);
    crete_process_files(hconfig);
    crete_process_stdin(hconfig);
}

void crete_preload_initialize(int argc, char**& argv)
{
    // Should exit program while being launched as prime
    crete_initialize(argc, argv);
    // Need to call crete_capture_begin before make_concolics, or they won't be captured.
    // crete_capture_end() is registered with atexit() in crete_initialize().
    crete_capture_begin();

    config::HarnessConfiguration hconfig = crete_load_configuration();
    crete_process_configuration(hconfig, argc, argv);
}

#if CRETE_HOST_ENV
bool crete_verify_executable_path_matches(const char* argv0)
{
    std::ifstream ifs(CRETE_CONFIG_SERIALIZED_PATH,
                      ios_base::in | ios_base::binary);

    if(!ifs.good())
    {
        throw std::runtime_error("failed to open file: " + std::string(CRETE_CONFIG_SERIALIZED_PATH));
    }

    boost::archive::text_iarchive ia(ifs);
    config::RunConfiguration config;
    ia >> config;

    return fs::equivalent(config.get_executable(), fs::path(argv0));
}
#endif // CRETE_HOST_ENV


// ********************************************************* //
// Signal handling
#define __INIT_CRETE_SIGNAL_HANDLER(SIGNUM)                                  \
        {                                               \
            struct sigaction sigact;                    \
                                                        \
            memset(&sigact, 0, sizeof(sigact));         \
            sigact.sa_handler = crete_signal_handler;   \
            sigaction(SIGNUM, &sigact, NULL);           \
                                                        \
            sigset_t set;                               \
            sigemptyset(&set);                          \
            sigaddset(&set, SIGNUM);                    \
            sigprocmask(SIG_UNBLOCK, &set, NULL);       \
        }

// Terminate the program gracefully and return the corresponding exit code
// TODO: xxx does not handle nested signals
static void crete_signal_handler(int signum)
{
    //TODO: xxx _exit() is safer, but atexit()/crete_capture_end() will not be called with _exit()
    exit(CRETE_EXIT_CODE_SIG_BASE + signum);
}

static void init_crete_signal_handlers(void)
{
    __INIT_CRETE_SIGNAL_HANDLER(SIGABRT);
    __INIT_CRETE_SIGNAL_HANDLER(SIGFPE);
    __INIT_CRETE_SIGNAL_HANDLER(SIGHUP);
    __INIT_CRETE_SIGNAL_HANDLER(SIGINT);
    __INIT_CRETE_SIGNAL_HANDLER(SIGILL);
    __INIT_CRETE_SIGNAL_HANDLER(SIGPIPE);
    __INIT_CRETE_SIGNAL_HANDLER(SIGQUIT);
    __INIT_CRETE_SIGNAL_HANDLER(SIGSEGV);
    __INIT_CRETE_SIGNAL_HANDLER(SIGSYS);
    __INIT_CRETE_SIGNAL_HANDLER(SIGTERM);
    __INIT_CRETE_SIGNAL_HANDLER(SIGTSTP);
    __INIT_CRETE_SIGNAL_HANDLER(SIGXCPU);
    __INIT_CRETE_SIGNAL_HANDLER(SIGXFSZ);

    // SIGUSR1 for timeout
    __INIT_CRETE_SIGNAL_HANDLER(SIGUSR1);
}
extern "C" {
    // The type of __libc_start_main
    typedef int (*__libc_start_main_t)(
            int *(main) (int, char**, char**),
            int argc,
            char ** ubp_av,
            void (*init) (void),
            void (*fini) (void),
            void (*rtld_fini) (void),
            void (*stack_end)
            );

    int __libc_start_main(
            int *(main) (int, char **, char **),
            int argc,
            char ** ubp_av,
            void (*init) (void),
            void (*fini) (void),
            void (*rtld_fini) (void),
            void *stack_end)
            __attribute__ ((noreturn));
}

int __libc_start_main(
        int *(main) (int, char **, char **),
        int argc,
        char ** ubp_av,
        void (*init) (void),
        void (*fini) (void),
        void (*rtld_fini) (void),
        void *stack_end) {

//    atexit(print_back_trace);

    __libc_start_main_t orig_libc_start_main;

    try
    {
        orig_libc_start_main = (__libc_start_main_t)dlsym(RTLD_NEXT, "__libc_start_main");
        if(orig_libc_start_main == 0)
            throw runtime_error("failed to find __libc_start_main");

#if CRETE_HOST_ENV
        // HACK (a bit of one)...
        // TODO: (crete-memcheck) research how to get around the problem of preloading
        // valgrind as well, in which case we check if the executable name matches.
        if(crete_verify_executable_path_matches(ubp_av[0]))
        {
            crete_preload_initialize(argc, ubp_av);
        }
#else
        init_crete_signal_handlers();
        crete_preload_initialize(argc, ubp_av);
#endif // CRETE_HOST_ENV
    }
    catch(exception& e)
    {
        cerr << "[CRETE] Exception: " << e.what() << endl;
        exit(1);
    }
    catch(...) // Non-standard exception
    {
        cerr << "[CRETE] Non-standard exception encountered!" << endl;
        assert(0);
    }

    (*orig_libc_start_main)(main, argc, ubp_av, init, fini, rtld_fini, stack_end);

    exit(1); // This is never reached. Doesn't matter, as atexit() is called upon returning from main().
}


#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BT_BUF_SIZE 100

void print_back_trace() {
    int j, nptrs;
    void *buffer[BT_BUF_SIZE];
    char **strings;

    nptrs = backtrace(buffer, BT_BUF_SIZE);
    fprintf(stderr, "=========================[preload]======================================\n");
    fprintf(stderr, "backtrace() returned %d addresses\n", nptrs);

    /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
       would produce similar output to the following: */

    strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
        perror("backtrace_symbols");
        exit(EXIT_FAILURE);
    }

    for (j = 0; j < nptrs; j++)
        fprintf(stderr, "%s\n", strings[j]);

    fprintf(stderr, "======================================================================\n");
    free(strings);
}
