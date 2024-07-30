
#if 0
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/net/WebSocket.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

#if defined(HYP_CURL) && HYP_CURL
#include <curl/curl.h>
#endif

namespace hyperion {

namespace net {

#pragma region WebSocketThread

WebSocketThread::WebSocketThread()
    : Thread(Name::Unique("WebSocketThread"))
{
}

void WebSocketThread::Stop()
{
    m_is_running.Set(false, MemoryOrder::RELAXED);
}

void WebSocketThread::operator()(WebSocket *websocket)
{
    m_is_running.Set(true, MemoryOrder::RELAXED);
    
    Queue<Scheduler::ScheduledTask> tasks;

    while (m_is_running.Get(MemoryOrder::RELAXED)) {
        if (uint32 num_enqueued = m_scheduler.NumEnqueued()) {
            m_scheduler.AcceptAll(tasks);

            while (tasks.Any()) {
                tasks.Pop().Execute();
            }
        }
    }

    // flush scheduler
    m_scheduler.Flush([](auto &operation)
    {
        operation.Execute();
    });
}

#pragma endregion WebSocketThread

#pragma region WebSocket

WebSocket::WebSocket(const String &url)
    : m_url(url),
      m_thread(new WebSocketThread)
{
#if defined(HYP_CURL) && HYP_CURL
    m_thread->Start(this);

    m_thread->GetSchedulerInstance()->Enqueue([this]()
    {
        HYP_SCOPE;

        HYP_LOG(Net, LogLevel::INFO, "WebSocket thread started");

        WebSocketThreadProc();
    }, TaskEnqueueFlags::FIRE_AND_FORGET);
#else
    HYP_LOG(Net, LogLevel::ERR, "cURL is not enabled in this build");
#endif
}

WebSocket::WebSocket(WebSocket &&other) noexcept
    : m_url(std::move(other.m_url)),
      m_thread(std::move(other.m_thread))
{
}

WebSocket &WebSocket::operator=(WebSocket &&other) noexcept
{
    if (this != &other) {
        m_url = std::move(other.m_url);
        m_thread = std::move(other.m_thread);
    }

    return *this;
}

WebSocket::~WebSocket()
{
    if (m_thread != nullptr) {
        if (m_thread->IsRunning()) {
            m_thread->Stop();
        }

        if (m_thread->CanJoin()) {
            m_thread->Join();
        }
    }
}

void WebSocket::WebSocketThreadProc()
{
    HYP_SCOPE;

    HYP_LOG(Net, LogLevel::INFO, "WebSocket thread started");

    CURL *curl = curl_easy_init();

    auto Ping = [=](CURL *curl, const char *payload)
    {
        size_t sent;
        CURLcode result = curl_ws_send(curl, payload, strlen(payload), &sent, 0, CURLWS_PING);

        return (int)result;
    };

    auto Pong = [=](CURL *curl, const char *payload)
    {
        size_t rlen;
        const struct curl_ws_frame *meta;
        char buffer[256];

        CURLcode result = curl_ws_recv(curl, buffer, sizeof(buffer), &rlen, &meta);

        if (!result) {
            if (meta->flags & CURLWS_PONG) {
                int same = 0;
                fprintf(stderr, "ws: got PONG back\n");

                if (rlen == strlen(payload)) {
                    if (!memcmp(payload, buffer, rlen)) {
                        fprintf(stderr, "ws: got the same payload back\n");
                        same = 1;
                    }
                }

                if (!same) {
                    fprintf(stderr, "ws: did NOT get the same payload back\n");
                }
            } else {
                fprintf(stderr, "recv_pong: got %u bytes rflags %x\n", (int)rlen, meta->flags);
            }
        }

        fprintf(stderr, "ws: curl_ws_recv returned %u, received %u\n", (unsigned int)result, (unsigned int)rlen);
        return (int)result;
    };

    auto OnData = [=](CURL *curl)
    {
        size_t rlen;
        const struct curl_ws_frame *meta;
        char buffer[256];

        return curl_ws_recv(curl, buffer, sizeof(buffer), &rlen, &meta);
    };

    auto Close = [=](CURL *curl)
    {
        size_t sent;
        (void)curl_ws_send(curl, "", 0, &sent, 0, CURLWS_CLOSE);
    };

    auto WebSocketMain = [=](CURL *curl)
    {
        while (true) {
            OnData(curl);

            if (Ping(curl, "ping")) {
                return;
            }

            if (Pong(curl, "pong")) {
                return;
            }

            Threads::Sleep(100);
        }

        Close(curl);
    };

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, m_url.Data());
        curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            HYP_LOG(Net, LogLevel::ERR, "cURL error: {}", curl_easy_strerror(res));
        } else {
            HYP_LOG(Net, LogLevel::INFO, "cURL request successful");

            WebSocketMain(curl);
        }

        curl_easy_cleanup(curl);
    }
}

#pragma endregion WebSocket

} // namespace net
} // namespace hyperion

#endif