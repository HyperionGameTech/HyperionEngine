/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <system/Debug.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT void Logger_Log(int log_level, const char *func_name, uint32 line, const char *message)
{
    DebugLog_(static_cast<LogType>(log_level), func_name != nullptr ? func_name : "", line, message);
}
} // extern "C"