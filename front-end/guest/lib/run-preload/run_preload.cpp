#include <crete/common.h>
#include <crete/custom_instr.h>
#include <crete/harness_config.h>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/filesystem.hpp>

#include <dlfcn.h>
#include <cassert>
#include <cstdlib>

#include <iostream>
#include <string>
#include <stdexcept>
#include <stdio.h>

using namespace std;
using namespace crete;
namespace fs = boost::filesystem;

void print_back_trace();

static inline config::HarnessConfiguration crete_load_configuration()
{
    fprintf(stderr, "crete_load_configuration() entered\n");

    std::ifstream ifs(CRETE_CONFIG_SERIALIZED_PATH,
                      ios_base::in | ios_base::binary);

    if(!ifs.good())
    {
        throw std::runtime_error("failed to open file: " + std::string(CRETE_CONFIG_SERIALIZED_PATH));
    }

    boost::archive::binary_iarchive ia(ifs);
    config::HarnessConfiguration config;

    fprintf(stderr, "before ia >> config\n");
    ia >> config;

    fprintf(stderr, "before return\n");

    return config;
}

static inline void crete_process_stdin_libc(const config::STDStream stdin_config)
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

static inline void crete_process_stdin(const config::HarnessConfiguration& hconfig)
{
    fprintf(stderr, "crete_process_stdin() entered\n");

    const config::STDStream stdin_config = hconfig.get_stdin();

    if(stdin_config.size > 0)
    {
        crete_process_stdin_libc(stdin_config);
    }
}

static inline void crete_make_concolic_file(const config::File& file)
{
    fprintf(stderr, "crete_make_concolic_file() entered\n");

    string filename = file.path.filename().string();

    assert(!filename.empty() && "[CRETE] file name must be valid");
    assert(file.size > 0 && "[CRETE] file size must be greater than zero");

    fprintf(stderr, "Create and initialize symbolic array: %llu bytes\n", file.size);
    char* buffer = new char [file.size];
    memset(buffer, 0, file.size);

    fprintf(stderr, "crete_make_concolic()\n");
    crete_make_concolic(buffer, file.size, filename.c_str());

    // write symbolic value to ramdisk file
    fprintf(stderr, "write symbolic value to ramdisk file\n");
    string file_full_path = fs::path(CRETE_RAMDISK_PATH / file.path.filename()).string();
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

    fprintf(stderr, "crete_make_concolic_file() finished\n");
}

static inline void crete_process_files(const config::HarnessConfiguration& hconfig)
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

static inline void crete_process_args(const config::Arguments& guest_config_args,
        int argc, char**& argv)
{
    fprintf(stderr, "crete_process_args() entered\n");

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

static inline void crete_process_configuration(const config::HarnessConfiguration& hconfig,
                                 int argc, char**& argv)
{
    fprintf(stderr, "crete_process_configuration() entered.\n");

    // Note: order matters.
    crete_process_args(hconfig.get_arguments(),argc, argv);
    crete_process_files(hconfig);
    crete_process_stdin(hconfig);
}

static inline void update_proc_maps()
{
    fprintf(stderr, "update_proc_maps() entered\n");

    namespace fs = boost::filesystem;

    // FIXME: xxx terrible way to identify the prime execution
    bool terminate = false;
    if(!fs::exists(CRETE_PROC_MAPS_PATH))
        terminate = true;

    fs::remove(CRETE_PROC_MAPS_PATH);
    ifstream ifs("/proc/self/maps");
    ofstream ofs(CRETE_PROC_MAPS_PATH);
    copy(istreambuf_iterator<char>(ifs),
         istreambuf_iterator<char>(),
         ostreambuf_iterator<char>(ofs));

    // This process must be running in order to get the proc-maps, so,
    // if we want to have crete-run relay the proc-map info, we need to run this once and terminate it before actually testing.
    if(terminate)
    {
        fprintf(stderr, "Early termination in update_proc_maps() (should be prime)\n");
        exit(0); // Normal operating procedure to get crete-run the proc-maps for this binary. Other proc-map updates are done for sanity check (to ensure ASLR is disabled).
    }
}

static inline void crete_preload_initialize(int argc, char**& argv)
{
    fprintf(stderr, "crete_preload_initialize() entered\n");

    // Should terminate program while being launched as prime
    update_proc_maps();

    atexit(crete_capture_end);
    // Need to call crete_capture_begin before make_concolics, or they won't be captured.
    crete_capture_begin();

    config::HarnessConfiguration hconfig = crete_load_configuration();
    crete_process_configuration(hconfig, argc, argv);

    fprintf(stderr, "crete_preload_initialize() finished\n");
}

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
static inline void crete_signal_handler(int signum)
{
    //TODO: xxx _exit() is safer, but atexit()/crete_capture_end() will not be called with _exit()
    exit(CRETE_EXIT_CODE_SIG_BASE + signum);
}

static inline void init_crete_signal_handlers(void)
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

        init_crete_signal_handlers();
        crete_preload_initialize(argc, ubp_av);
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
