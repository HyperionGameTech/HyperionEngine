/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/threading/TaskThread.hpp>

#include <Core/memory/RefCountedPtr.hpp>

namespace Hyperion {
namespace net {

class CORE_API NetRequestThread final : public TaskThread
{
public:
    NetRequestThread();
    virtual ~NetRequestThread() override;
};

CORE_API void SetGlobalNetRequestThread(const RC<NetRequestThread>& netRequestThread);
CORE_API const RC<NetRequestThread>& GetGlobalNetRequestThread();

} // namespace net

using net::GetGlobalNetRequestThread;
using net::NetRequestThread;
using net::SetGlobalNetRequestThread;

} // namespace Hyperion
