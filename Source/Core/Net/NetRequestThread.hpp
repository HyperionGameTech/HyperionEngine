/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Threading/TaskThread.hpp>

#include <Core/Memory/SharedPtr.hpp>

namespace Hyperion {
namespace net {

class CORE_API NetRequestThread final : public TaskThread
{
public:
    NetRequestThread();
    virtual ~NetRequestThread() override;
};

CORE_API void SetGlobalNetRequestThread(const SharedPtr<NetRequestThread>& netRequestThread);
CORE_API const SharedPtr<NetRequestThread>& GetGlobalNetRequestThread();

} // namespace net

using net::GetGlobalNetRequestThread;
using net::NetRequestThread;
using net::SetGlobalNetRequestThread;

} // namespace Hyperion
