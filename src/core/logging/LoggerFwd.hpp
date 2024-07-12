/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UTIL_LOGGER_FWD_HPP
#define HYPERION_UTIL_LOGGER_FWD_HPP

namespace hyperion {

namespace logging {
class LogChannel;
} // namespace logging

using logging::LogChannel;

} // namespace hyperion

// Helper macros
#define HYP_DECLARE_LOG_CHANNEL(name) \
    extern hyperion::logging::LogChannel Log_##name

#ifndef HYP_LOG
    #define HYP_LOG(...)
#endif

#endif