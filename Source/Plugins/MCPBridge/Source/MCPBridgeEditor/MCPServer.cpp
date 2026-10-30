/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include "MCPServer.hpp"
#include "MCPProtocol.hpp"
#include "MCPJsonConvert.hpp"

#include <Core/Core.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Core/Threading/Threads.hpp>

#include <chrono>
#include <cstdlib>
#include <utility>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using SocketSize = int;
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
using SOCKET = int;
using SocketSize = socklen_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket ::close
#endif

namespace Hyperion {
namespace MCP {

HYP_DEFINE_LOG_CHANNEL(MCP);

namespace {

constexpr uint16 DefaultPort = 2673;

constexpr uint32 MaxLogLines = MCPMaxLogLines;
constexpr size_t MaxHeaderBytes = 32u * 1024u;                          // request head (line + headers)
constexpr size_t MaxBodyBytes = 16u * 1024u * 1024u;                    // request body cap
constexpr uint32 RequestReadTimeoutMs = 15 * 1000;                      // whole request (head + body) must arrive within this
constexpr uint64 AcceptPollIntervalMs = 50;
constexpr uint64 SelectTimeoutUs = 200 * 1000;                          // poll m_running periodically

CommandLineArgumentRegistration g_argMcpPort {
    "mcpport",
    {},
    "Port the MCP bridge listens on (overrides HYPERION_MCP_PORT)",
    CommandLineArgumentFlags::NONE,
    CommandLineArgumentType::INTEGER
};

uint16 ResolvePort()
{
    uint16 port = DefaultPort;

    if (const char* value = std::getenv("HYPERION_MCP_PORT"))
    {
        int parsed = atoi(value);
        if (parsed > 0 && parsed < 65536)
        {
            port = uint16(parsed);
        }
    }

    if (const CommandLineArgumentValue portArgument = CoreApi::ParseCommandLineArguments()["mcpport"]; portArgument.IsNumber())
    {
        const int32 parsed = portArgument.ToInt32();

        if (parsed > 0 && parsed < 65536)
        {
            port = uint16(parsed);
        }
    }

    return port;
}

void CloseSocketChecked(SOCKET& socket)
{
    if (socket != INVALID_SOCKET)
    {
        closesocket(socket);
        socket = INVALID_SOCKET;
    }
}

bool IsLoopbackAuthority(const String& authority)
{
    String host = authority;

    if (authority.StartsWith("["))
    {
        const size_t close = authority.FindFirstIndex(']');

        if (close == String::NotFound)
        {
            return false;
        }

        host = String(authority.Substr(0, close + 1));
    }
    else if (const size_t colon = authority.FindFirstIndex(':'); colon != String::NotFound)
    {
        host = String(authority.Substr(0, colon));
    }

    return host == "localhost"
        || host == "127.0.0.1"
        || host == "[::1]";
}

bool SendAll(int socket, const char* data, size_t size)
{
    size_t offset = 0;

    while (offset < size)
    {
#ifdef HYP_WINDOWS
        int sent = ::send(socket, data + offset, int(size - offset), 0);
#else
        ssize_t sent = ::send(socket, data + offset, size - offset, MSG_NOSIGNAL);
#endif
        if (sent <= 0)
        {
            return false;
        }

        offset += size_t(sent);
    }

    return true;
}

const char* StatusText(int statusCode)
{
    switch (statusCode)
    {
    case 200: return "OK";
    case 202: return "Accepted";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 413: return "Payload Too Large";
    default: return "OK";
    }
}

void SendResponse(int socket, int statusCode, const char* contentType, const String& body)
{
    char header[256];

#ifdef HYP_WINDOWS
    sprintf_s(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %llu\r\n"
        "Connection: close\r\n"
        "\r\n",
        statusCode, StatusText(statusCode), contentType, (unsigned long long)body.Size());
#else
    snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %llu\r\n"
        "Connection: close\r\n"
        "\r\n",
        statusCode, StatusText(statusCode), contentType, (unsigned long long)body.Size());
#endif

    SendAll(socket, header, strlen(header));

    if (!body.Empty())
    {
        SendAll(socket, body.Data(), body.Size());
    }
}

} // namespace

MCPServer& MCPServer::GetInstance()
{
    static MCPServer s_instance;

    return s_instance;
}

bool MCPServer::AutoStart()
{
    return Start(ResolvePort());
}

bool MCPServer::Start(uint16 port)
{
    if (m_running.Load())
    {
        return true;
    }

    InstallLogRedirect();

    m_running.Store(true);

    m_thread = MakeUnique<MCPServerThread>(this);
    m_thread->Start();

    m_thread->GetScheduler().Enqueue(
        [this, port]()
        {
            ServerThreadProc(port);
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);

    HYP_LOG(MCP, Info, "MCP bridge listening on http://127.0.0.1:{}/mcp", port);

    return true;
}

void MCPServer::Stop()
{
    if (!m_running.Store(false))
    {
        return; // was not running
    }

    SOCKET listenSocketCopy = SOCKET(m_listenSocket.Exchange(-1, MemoryOrder::ACQUIRE_RELEASE));
    CloseSocketChecked(listenSocketCopy);

    if (m_thread != nullptr)
    {
        if (m_thread->IsRunning())
        {
            m_thread->Stop();
        }

        if (m_thread->CanJoin())
        {
            m_thread->Join();
        }

        m_thread.Reset();
    }

    RemoveLogRedirect();

    HYP_LOG(MCP, Info, "MCP bridge stopped");
}

Array<MCPLogLine> MCPServer::GetLogLines(uint32 maxLines, int minLevel) const
{
    Array<MCPLogLine> outLines;

    Mutex::Guard guard(m_logMutex);

    const uint32 count = uint32(m_logLines.Size());
    const uint32 start = count > maxLines ? count - maxLines : 0;

    for (uint32 i = start; i < count; i++)
    {
        if (m_logLines[i].level > minLevel)
        {
            continue;
        }

        outLines.PushBack(m_logLines[i]);
    }

    return outLines;
}

void MCPServer::InstallLogRedirect()
{
    if (m_logRedirectId != -1)
    {
        return;
    }

    m_logRedirectId = Logger::GetInstance().AddRedirect(
        Bitset(uint64(-1)), // all channels
        this,
        &MCPServer::LogRedirectProc,
        &MCPServer::LogRedirectProc);
}

void MCPServer::RemoveLogRedirect()
{
    if (m_logRedirectId == -1)
    {
        return;
    }

    Logger::GetInstance().RemoveRedirect(m_logRedirectId);
    m_logRedirectId = -1;
}

bool MCPServer::LogRedirectProc(void* context, const LogChannel& channel, const LogMessage& message)
{
    auto* server = static_cast<MCPServer*>(context);

    String text;
    for (const StringView<StringType::UTF8>& chunk : message.chunks)
    {
        text.Append(chunk);
    }

    MCPLogLine line;
    line.timestampMs = message.timestamp;
    line.level = int(message.level);
    line.channel = ANSIString(channel.name.LookupString());
    line.text = std::move(text);

    {
        Mutex::Guard guard(server->m_logMutex);
        server->m_logLines.PushBack(std::move(line));

        while (server->m_logLines.Size() > MaxLogLines)
        {
            server->m_logLines.Erase(server->m_logLines.Begin());
        }
    }

    // Allow default logging to continue.
    return true;
}

void MCPServer::ServerThreadProc(uint16 port)
{
#ifdef HYP_WINDOWS
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        HYP_LOG(MCP, Error, "WSAStartup failed");

        m_running.Store(false);

        return;
    }
#endif

    SOCKET listenSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET)
    {
        HYP_LOG(MCP, Error, "Failed to create listen socket");

        m_running.Store(false);

#ifdef HYP_WINDOWS
        WSACleanup();
#endif
        return;
    }

    {
        int reuse = 1;
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
    }

    {
        sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = inet_addr("127.0.0.1");
        address.sin_port = htons(port);

        if (::bind(listenSocket, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
        {
            HYP_LOG(MCP, Warning, "Failed to bind 127.0.0.1:{} - MCP bridge not started (port in use?)", port);
            CloseSocketChecked(listenSocket);

            m_running.Store(false);

#ifdef HYP_WINDOWS
            WSACleanup();
#endif
            return;
        }
    }

    if (::listen(listenSocket, 8) == SOCKET_ERROR)
    {
        HYP_LOG(MCP, Warning, "Failed to listen on 127.0.0.1:{} - MCP bridge not started", port);
        CloseSocketChecked(listenSocket);

        m_running.Store(false);

#ifdef HYP_WINDOWS
        WSACleanup();
#endif
        return;
    }

    // Non-blocking accept loop so we can shut down cleanly.
#ifdef HYP_WINDOWS
    u_long nonBlocking = 1;
    ioctlsocket(listenSocket, FIONBIO, &nonBlocking);
#else
    fcntl(listenSocket, F_SETFL, fcntl(listenSocket, F_GETFL, 0) | O_NONBLOCK);
#endif

    m_listenSocket.Set(intptr_t(listenSocket), MemoryOrder::RELEASE);

    // One connection per HTTP request; connections are served serially from the backlog.
    while (m_running.Load())
    {
        sockaddr_in clientAddress {};
        SocketSize clientAddressSize = sizeof(clientAddress);

        SOCKET clientSocket = ::accept(listenSocket, (sockaddr*)&clientAddress, &clientAddressSize);

        if (clientSocket == INVALID_SOCKET)
        {
            ThreadSleep(uint32(AcceptPollIntervalMs));
            continue;
        }

        HandleConnection(int(clientSocket));

        closesocket(clientSocket);
    }

#ifdef HYP_WINDOWS
    WSACleanup();
#endif
}

void MCPServer::HandleConnection(int clientSocket)
{
    // Connections are served one at a time, so a client that stalls mid-request must not hold the bridge forever.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(RequestReadTimeoutMs);

    // Read the request head.
    ANSIString buffer;
    char readBuffer[8192];

    size_t headerEnd = size_t(-1);

    while (m_running.Load())
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            SendResponse(clientSocket, 408, "application/json", String("{\"error\":\"request timed out\"}"));

            return;
        }

        // Look for the \r\n\r\n terminator.
        if (buffer.Size() >= 4)
        {
            const char* data = buffer.Data();

            for (size_t i = 0; i + 4 <= buffer.Size(); i++)
            {
                if (data[i] == '\r' && data[i + 1] == '\n' && data[i + 2] == '\r' && data[i + 3] == '\n')
                {
                    headerEnd = i;
                    break;
                }
            }

            if (headerEnd != size_t(-1))
            {
                break;
            }
        }

        if (buffer.Size() > MaxHeaderBytes)
        {
            SendResponse(clientSocket, 413, "application/json", String("{\"error\":\"request head too large\"}"));

            return;
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(clientSocket, &readSet);

        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = long(SelectTimeoutUs);

        int ready = ::select(clientSocket + 1, &readSet, nullptr, nullptr, &timeout);
        if (ready <= 0)
        {
            if (!m_running.Load())
            {
                return;
            }

            continue;
        }

#ifdef HYP_WINDOWS
        int received = ::recv(clientSocket, readBuffer, sizeof(readBuffer), 0);
#else
        ssize_t received = ::recv(clientSocket, readBuffer, sizeof(readBuffer), 0);
#endif

        if (received <= 0)
        {
            return;
        }

        buffer.Append(readBuffer, readBuffer + received);
    }

    if (!m_running.Load() || headerEnd == size_t(-1))
    {
        return;
    }

    // Parse the request line: METHOD SP PATH SP VERSION
    const ANSIStringView head(buffer.Data(), buffer.Data() + headerEnd);
    const size_t lineEnd = head.FindFirstIndex('\n');
    if (lineEnd == ANSIStringView::NotFound)
    {
        SendResponse(clientSocket, 400, "application/json", String("{\"error\":\"malformed request\"}"));

        return;
    }

    ANSIStringView requestLine(head.Data(), head.Data() + lineEnd);
    if (requestLine.Size() > 0 && requestLine.Data()[requestLine.Size() - 1] == '\r')
    {
        requestLine = ANSIStringView(requestLine.Data(), requestLine.Data() + requestLine.Size() - 1);
    }

    const size_t methodEnd = requestLine.FindFirstIndex(' ');
    if (methodEnd == ANSIStringView::NotFound)
    {
        SendResponse(clientSocket, 400, "application/json", String("{\"error\":\"malformed request line\"}"));

        return;
    }

    const ANSIStringView method(requestLine.Data(), requestLine.Data() + methodEnd);

    if (method == ANSIStringView("GET"))
    {
        // No server-initiated SSE stream - 405 is the spec-compliant signal.
        SendResponse(clientSocket, 405, "application/json", String("{\"error\":\"GET not supported (no SSE stream)\"}"));

        return;
    }

    if (!(method == ANSIStringView("POST")))
    {
        SendResponse(clientSocket, 405, "application/json", String("{\"error\":\"method not allowed\"}"));

        return;
    }

    // Find Content-Length in the headers. Origin / Host are checked as well: a web page open in the
    // user's browser (or a DNS rebinding attack) must not be able to drive the editor through this port.
    size_t contentLength = 0;
    bool originAllowed = true; // no Origin header = not a browser request
    bool hostAllowed = true;

    {
        const ANSIStringView headers(head.Data() + lineEnd + 1, head.Data() + head.Size());

        size_t offset = 0;
        while (offset < headers.Size())
        {
            size_t lineStart = offset;
            size_t end = ANSIStringView(headers.Data() + offset, headers.Data() + headers.Size()).FindFirstIndex('\n');

            if (end == ANSIStringView::NotFound)
            {
                end = headers.Size() - offset;
            }

            ANSIStringView headerLine(headers.Data() + lineStart, headers.Data() + lineStart + end);

            // trim \r
            if (headerLine.Size() > 0 && headerLine.Data()[headerLine.Size() - 1] == '\r')
            {
                headerLine = ANSIStringView(headerLine.Data(), headerLine.Data() + headerLine.Size() - 1);
            }

            String lowerLine = String(headerLine.Data(), headerLine.Data() + headerLine.Size()).ToLower();

            if (lowerLine.StartsWith("origin:"))
            {
                const String origin = String(lowerLine.Substr(String("origin:").Length())).Trimmed();

                if (origin.StartsWith("http://"))
                {
                    originAllowed = IsLoopbackAuthority(String(origin.Substr(String("http://").Length())));
                }
                else if (origin.StartsWith("https://"))
                {
                    originAllowed = IsLoopbackAuthority(String(origin.Substr(String("https://").Length())));
                }
                else
                {
                    originAllowed = false;
                }
            }
            else if (lowerLine.StartsWith("host:"))
            {
                hostAllowed = IsLoopbackAuthority(String(lowerLine.Substr(String("host:").Length())).Trimmed());
            }
            else if (lowerLine.StartsWith("content-length:"))
            {
                String value = String(lowerLine.Substr(String("content-length:").Length())).Trimmed();

                int64 parsed = 0;
                if (StringUtil::Parse(value, &parsed))
                {
                    contentLength = size_t(parsed);
                }
            }

            offset = lineStart + end + 1;
        }
    }

    if (!originAllowed || !hostAllowed)
    {
        SendResponse(clientSocket, 403, "application/json", String("{\"error\":\"forbidden origin or host\"}"));

        return;
    }

    if (contentLength > MaxBodyBytes)
    {
        SendResponse(clientSocket, 413, "application/json", String("{\"error\":\"payload too large\"}"));

        return;
    }

    // Read the body (part of it may already be in the buffer).
    ANSIString body(buffer.Data() + headerEnd + 4, buffer.Data() + buffer.Size());

    while (body.Size() < contentLength && m_running.Load())
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            SendResponse(clientSocket, 408, "application/json", String("{\"error\":\"request timed out\"}"));

            return;
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(clientSocket, &readSet);

        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = long(SelectTimeoutUs);

        int ready = ::select(clientSocket + 1, &readSet, nullptr, nullptr, &timeout);
        if (ready <= 0)
        {
            if (!m_running.Load())
            {
                return;
            }

            continue;
        }

#ifdef HYP_WINDOWS
        int received = ::recv(clientSocket, readBuffer, sizeof(readBuffer), 0);
#else
        ssize_t received = ::recv(clientSocket, readBuffer, sizeof(readBuffer), 0);
#endif

        if (received <= 0)
        {
            return;
        }

        body.Append(readBuffer, readBuffer + received);
    }

    HandleHttpRequest(clientSocket, body);
}

bool MCPServer::HandleHttpRequest(int clientSocket, const ANSIString& body)
{
    JSON::ParseResult parseResult = JSON::Parse(UTF8StringView(body.Data(), body.Data() + body.Size()));
    if (!parseResult.ok)
    {
        SendResponse(clientSocket, 400, "application/json",
            String("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}"));

        return false;
    }

    if (!parseResult.value.IsObject())
    {
        SendResponse(clientSocket, 400, "application/json", String("{\"error\":\"expected JSON object\"}"));

        return false;
    }

    JSON::Object response;
    if (!HandleMessage(parseResult.value.AsObject(), response))
    {
        // Notification or otherwise no response expected.
        SendResponse(clientSocket, 202, "application/json", String());

        return false;
    }

    SendResponse(clientSocket, 200, "application/json", WriteJson(JSON::Value(std::move(response))));

    return true;
}

} // namespace MCP
} // namespace Hyperion
