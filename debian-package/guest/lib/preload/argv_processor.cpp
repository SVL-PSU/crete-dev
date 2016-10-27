#include "argv_processor.h"

#include <crete/harness.h>
#include <crete/custom_instr.h>

#include <boost/property_tree/ptree_serialization.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp> // Needed for text_iarchive (for some reason).
#include <boost/property_tree/xml_parser.hpp>

#include <cassert>
#include <cstdlib>

#include <iostream>
#include <string>
#include <stdexcept>

#include<sstream>

using namespace std;

namespace crete
{
namespace config
{

static int crete_argc_handle = 0;
static char** crete_argv_handle = NULL;

static void crete_clean_up_argv()
{
    if(crete_argv_handle == NULL)
        return;

    for(int i = 0; i < crete_argc_handle; ++i)
    {
        free(crete_argv_handle[i]);
    }

    free(crete_argv_handle);
}

void init_to_null(char** v, int size)
{
    for(int i = 0; i < size; ++i)
    {
        v[i] = NULL;
    }
}

bool lt_index(const Argument& lhs, const Argument& rhs)
{
    return lhs.index < rhs.index;
}

bool find_index_zero(const Argument& arg)
{
    if(arg.index == 0)
        return true;
    return false;
}

bool has_index_zero(const Arguments& args)
{
    return std::find_if(args.begin(), args.end(), find_index_zero) != args.end();
}

int adjust_for_index_zero(int argc,
                          const Arguments& args)
{
    if(args.size() > 0 && !has_index_zero(args))
    {
        ++argc;
    }

    return argc;
}

int determine_argc_value(int argc,
                         const Arguments& args)
{
    int new_argc = 0;

    new_argc = argc > (int)args.size() ? argc : (int)args.size();
    new_argc = adjust_for_index_zero(new_argc, args);

    return new_argc;
}

void restore_original(char** argv,
                      char** argv_orig,
                      const int argc_orig)
{
    for(int i = 0; i < argc_orig; ++i)
    {
        size_t size = strlen(argv_orig[i]) + 1;
        argv[i] = (char*)malloc(size);
        strcpy(argv[i], argv_orig[i]);
    }
}

void verify_invariants(const Argument& arg,
                       int argc)
{
    if(arg.size == 0)
    {
        throw std::runtime_error("argument size is 0");
    }
    if(arg.index >= static_cast<std::size_t>(argc))
    {
        throw std::runtime_error("arg index outside of bounds");
    }
}

std::size_t adjust_for_null_term(std::size_t size)
{
    return size + 1;
}

char* allocate_arg(char* arg,
                   std::size_t size)
{
    if(arg != NULL)
    {
        free(arg);
    }

    return (char*)malloc(size);
}

void copy_value(char* varg,
          const Argument& arg)
{
    strncpy(varg, arg.value.c_str(), arg.size);
}

char* init_arg(char* varg,
               const Argument& arg,
               std::size_t mem_size,
               std::string& concolic_name,
               bool first_iteration)
{
    assert(mem_size > 0);

    if(arg.concolic)
    {
        stringstream concolic_argv_name;
        concolic_argv_name << "argv_" << arg.index;

        crete_make_concolic(&varg[0], arg.size, concolic_argv_name.str().c_str());
    }
    else
    {
        copy_value(varg, arg);
    }

    varg[mem_size - 1] = '\0';

    return varg;
}

void fill_argv(char** argv,
               const Arguments& args,
               const int argc,
               bool first_iteration)
{
    std::string name = "arg";
    for(Arguments::const_iterator it = args.begin();
        it != args.end();
        ++it)
    {
        const config::Argument& arg = *it;

        verify_invariants(arg, argc);

        std::size_t index = arg.index;
        std::size_t mem_size = adjust_for_null_term(arg.size);

        argv[index] = allocate_arg(argv[index], mem_size);

        argv[index] = init_arg(argv[index],
                               arg,
                               mem_size,
                               name,
                               first_iteration);
    }
}

// FIXME: xxx Refactor after launching program with correct arguments within crete-run
void process_argv(const Arguments& config_args,
                  bool first_iteration,
                  int& argc,
                  char**& argv)
{
    assert(argc > 0 && "[CRETE] - argc should always be greater than 0");

    char** argv_old = argv;
    const int argc_old = argc;

    argc = determine_argc_value(argc, config_args);
    argv = (char**)malloc((argc + 1) * sizeof(char*));

    argv[argc] = NULL;

    init_to_null(argv, argc);

    atexit(crete_clean_up_argv);

    restore_original(argv, argv_old, argc_old);
    fill_argv(argv, config_args, argc, first_iteration);

    crete_argc_handle = argc;
    crete_argv_handle = argv;
}

} // namespace config
} // namespace crete
