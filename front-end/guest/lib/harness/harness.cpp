#include "harness.h"

#include <crete/harness.h>
#include <crete/custom_instr.h>

#include <fstream>
#include <memory>
#include <iostream>

#include <boost/filesystem.hpp>

using namespace std;

auto_ptr<crete::Harness> g_harness;
int g_argc = 0;
char** g_argv = NULL;

void crete_initialize(int argc, char* argv[])
{
    using namespace crete;

    try
    {
        if(argc < 1)
            throw runtime_error("missing executable name from argv!");

        g_argc = argc;
        g_argv = argv;

        g_harness.reset(new Harness(argv[0]));

        atexit(crete_capture_end);
    }
    catch(exception& e)
    {
        std::cerr << "CRETE Exception: " << e.what() << endl;
        exit(1);
    }
    catch(...) // Non-standard exception
    {
        assert(0);
    }
}

int crete_start(int (*harness)(int argc, char* argv[]))
{
    assert(g_harness.get() && "Must call crete_initialize first!");

    crete_capture_begin();
    int ret = harness(g_argc, g_argv);
    crete_capture_end();

    return ret;
}

namespace crete
{

Harness::Harness(string exe_name) :
    exe_name_(exe_name)
{
    update_proc_maps();
}

void Harness::update_proc_maps()
{
    namespace fs = boost::filesystem;

    bool terminate = false;
    if(!fs::exists(PROC_MAPS_FILE_NAME)) // proc-maps file doesn't exist.
        terminate = true;

    fs::remove(PROC_MAPS_FILE_NAME);
//    fs::copy_file("/proc/self/maps", PROC_MAPS_FILE_NAME); // TODO: getting Exception: Bad file descriptor when using this...
    ifstream ifs("/proc/self/maps");
    ofstream ofs(PROC_MAPS_FILE_NAME.c_str());
    copy(istreambuf_iterator<char>(ifs),
         istreambuf_iterator<char>(),
         ostreambuf_iterator<char>(ofs));


    // This process must be running in order to get the proc-maps, so, if we want to have crete-run relay the proc-map info, we need to run this once and terminate it before actually testing.
    if(terminate)
        exit(0); // Normal operating proceedure to get crete-run the proc-maps for this binary. Other proc-map updates are done for sanity check (to ensure ASLR is disabled).
}

}
