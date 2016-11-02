#ifndef CRETE_DISPATCH_COMMON_H
#define CRETE_DISPATCH_COMMON_H

#include <crete/logger.h>
#include <crete/logger_attr.h>

#define DISPATCH_LOG_TIME_FUNC() \
    crete::log::Scope<crete::log::Stopwatch> sswatch(crete::log::Stopwatch(), \
                         crete::RegistryMap<crete::log::Logger>::get(crete::LOG_DISPATCH_PROFILE), \
                         string(__func__), \
                         true)

namespace crete
{

const char* const LOG_DISPATCH_PROFILE = "profile";

}

#endif // CRETE_DISPATCH_COMMON_H
