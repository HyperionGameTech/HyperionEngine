/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Memory/ByteBuffer.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Core/Utilities/Span.hpp>
#include <Core/Utilities/Optional.hpp>

#include <Core/Functional/Proc.hpp>
#include <Core/Functional/Delegate.hpp>

#include <Core/Threading/Task.hpp>
#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/TaskThread.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
namespace net {

CORE_API void SetGlobalHTTPRequestThread(const SharedPtr<TaskThread>& thread);
CORE_API const SharedPtr<TaskThread>& GetGlobalHTTPRequestThread();

enum class HTTPMethod
{
    GET,
    POST,
    PUT,
    PATCH,
    DELETE_
};

class CORE_API HTTPResponse
{
public:
    friend class HTTPRequest;

    HTTPResponse();
    HTTPResponse(const HTTPResponse& other) = delete;
    HTTPResponse& operator=(const HTTPResponse& other) = delete;
    HTTPResponse(HTTPResponse&& other) noexcept;
    HTTPResponse& operator=(HTTPResponse&& other) noexcept = delete;
    ~HTTPResponse();

    HYP_FORCE_INLINE int GetStatusCode() const
    {
        return m_statusCode;
    }

    HYP_FORCE_INLINE bool IsSuccess() const
    {
        return m_statusCode >= 200 && m_statusCode < 400;
    }

    HYP_FORCE_INLINE bool IsError() const
    {
        return m_statusCode >= 400;
    }

    HYP_FORCE_INLINE const ByteBuffer& ToByteBuffer() const
    {
        return m_body;
    }

    Optional<JSON::Value> ToJSON() const;

    void OnDataReceived(Span<char> data);
    void OnComplete(int statusCode);

    Delegate<void, Span<char>> OnDataReceivedDelegate;
    Delegate<void, int> OnCompleteDelegate;

private:
    int m_statusCode;
    ByteBuffer m_body;
    mutable Mutex m_mutex;
};

class CORE_API HTTPRequest
{
public:
    HTTPRequest(const String& url, HTTPMethod method = HTTPMethod::GET);
    HTTPRequest(const String& url, const JSON::Value& body, HTTPMethod method = HTTPMethod::GET);
    HTTPRequest(const HTTPRequest& other);
    HTTPRequest& operator=(const HTTPRequest& other);
    HTTPRequest(HTTPRequest&& other) noexcept;
    HTTPRequest& operator=(HTTPRequest&& other) noexcept;
    ~HTTPRequest();

    HYP_FORCE_INLINE const String& GetURL() const
    {
        return m_url;
    }

    HYP_FORCE_INLINE HTTPMethod GetMethod() const
    {
        return m_method;
    }

    HYP_NODISCARD Task<HTTPResponse> Send();

private:
    String m_url;
    HTTPMethod m_method;
    ByteBuffer m_body;
    String m_contentType;
};

} // namespace net

using net::GetGlobalHTTPRequestThread;
using net::HTTPMethod;
using net::HTTPRequest;
using net::HTTPResponse;
using net::SetGlobalHTTPRequestThread;

} // namespace Hyperion
