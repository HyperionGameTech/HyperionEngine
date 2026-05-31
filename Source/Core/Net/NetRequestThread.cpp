/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Net/NetRequestThread.hpp>

#include <Core/Threading/Mutex.hpp>

namespace Hyperion::net {

static RC<NetRequestThread> g_globalNetRequestThread;
static Mutex g_globalNetRequestThreadMutex;

CORE_API void SetGlobalNetRequestThread(const RC<NetRequestThread>& netRequestThread)
{
    Mutex::Guard guard(g_globalNetRequestThreadMutex);

    g_globalNetRequestThread = netRequestThread;
}

CORE_API const RC<NetRequestThread>& GetGlobalNetRequestThread()
{
    Mutex::Guard guard(g_globalNetRequestThreadMutex);

    return g_globalNetRequestThread;
}

NetRequestThread::NetRequestThread()
    : TaskThread(NAME("NetRequestThread"), ThreadPriorityValue::LOWEST)
{
}

NetRequestThread::~NetRequestThread() = default;

} // namespace Hyperion::net
