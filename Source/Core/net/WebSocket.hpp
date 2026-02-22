/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#if 0

#include <Core/Defines.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/containers/String.hpp>

#include <Core/threading/Thread.hpp>
#include <Core/threading/Scheduler.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
namespace net {

class WebSocket;

class HYP_API WebSocketThread final : public Thread<Scheduler, WebSocket *>
{
public:
    WebSocketThread();
    virtual ~WebSocketThread() override = default;

    void Stop();

    /*! \brief Atomically load the boolean value indicating that this thread is actively running */
    bool IsRunning() const
        { return m_isRunning.Load(); }

private:
    virtual void operator()(WebSocket *) override;

    AtomicVar<bool> m_isRunning;
};

class HYP_API WebSocket
{
public:
    WebSocket(const String &url);
    WebSocket(const WebSocket &other)               = delete;
    WebSocket &operator=(const WebSocket &other)    = delete;
    WebSocket(WebSocket &&other) noexcept;
    WebSocket &operator=(WebSocket &&other) noexcept;
    ~WebSocket();

    HYP_FORCE_INLINE const String &GetURL() const
        { return m_url; }

private:
    void WebSocketThreadProc();

    String                      m_url;
    UniquePtr<WebSocketThread>  m_thread;
};

} // namespace net

using net::WebSocket;
using net::WebSocketThread;

} // namespace Hyperion

#endif
