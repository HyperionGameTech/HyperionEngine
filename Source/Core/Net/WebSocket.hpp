/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Threading/Thread.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
namespace net {

class WebSocket;

class CORE_API WebSocket
{
public:
    WebSocket(const String &url);
    
    WebSocket(const WebSocket &other) = delete;
    WebSocket& operator=(const WebSocket &other) = delete;

    WebSocket(WebSocket &&other) noexcept;
    WebSocket &operator=(WebSocket &&other) noexcept;
    
    ~WebSocket();

    HYP_FORCE_INLINE const String& GetURL() const
        { return m_url; }

private:
    void WebSocketThreadProc();

    String m_url;
    UniquePtr<ThreadBase> m_thread;
};

} // namespace net

using net::WebSocket;

} // namespace Hyperion

