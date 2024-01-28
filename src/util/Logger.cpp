#include <util/Logger.hpp>

#include <core/lib/Mutex.hpp>
#include <core/lib/AtomicVar.hpp>
#include <core/lib/HashMap.hpp>

namespace hyperion {

class LogChannelIDGenerator
{
public:
    LogChannelIDGenerator()                                                     = default;
    LogChannelIDGenerator(const LogChannelIDGenerator &other)                   = delete;
    LogChannelIDGenerator &operator=(const LogChannelIDGenerator &other)        = delete;
    LogChannelIDGenerator(LogChannelIDGenerator &&other) noexcept               = delete;
    LogChannelIDGenerator &operator=(LogChannelIDGenerator &&other) noexcept    = delete;
    ~LogChannelIDGenerator()                                                    = default;

    UInt32 ForName(Name name)
    {
        Mutex::Guard guard(mutex);

        auto it = name_map.Find(name);

        if (it != name_map.End()) {
            return it->second;
        }

        const UInt32 id = id_counter++;

        name_map.Insert(name, id);

        return id;
    }

private:
    UInt32                  id_counter = 0u;
    Mutex                   mutex;
    HashMap<Name, UInt64>   name_map;
};

static LogChannelIDGenerator g_log_channel_id_generator { };

// LogChannel

LogChannel::LogChannel(Name name)
    : id(g_log_channel_id_generator.ForName(name)),
      name(name)
{
}

// Logger

Logger::Logger()
    : m_context_name(Name::Unique("logger")),
      m_log_mask(~0u)
{
}

Logger::Logger(Name context_name)
    : m_context_name(context_name),
      m_log_mask(~0u)
{
}

} // namespace hyperion