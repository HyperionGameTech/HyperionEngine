#include <system/Debug.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    void Logger_Log(int log_level, const char *func_name, UInt32 line, const char *message)
    {
        DebugLog_(static_cast<LogType>(log_level), func_name, line, message);
    }
}