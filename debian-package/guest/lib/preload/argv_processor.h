#ifndef CRETE_ARGV_PROCESSOR_H
#define CRETE_ARGV_PROCESSOR_H

#include <crete/harness_config.h>

namespace crete
{
namespace config
{

void process_argv(const Arguments& config_args,
                  bool first_iteration,
                  int& argc,
                  char**& argv);

} // namespace config
} // namespace crete

#endif // CRETE_ARGV_PROCESSOR_H
