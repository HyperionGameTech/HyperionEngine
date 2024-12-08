/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/net/HTTPRequest.hpp>
#include <core/net/NetRequestThread.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

#if defined(HYP_CURL) && HYP_CURL
#include <curl/curl.h>
#endif

namespace hyperion {

namespace net {

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    HTTPResponse *response = static_cast<HTTPResponse *>(userp);
    const size_t realsize = size * nmemb;

    response->OnDataReceived(Span<char>(static_cast<char *>(contents), realsize));

    return realsize;
}

#pragma region HTTPResponse

HTTPResponse::HTTPResponse()
    : m_status_code(0)
{
}

HTTPResponse::HTTPResponse(HTTPResponse &&other) noexcept
    : m_body(std::move(other.m_body)),
      m_status_code(other.m_status_code),
      OnDataReceivedDelegate(std::move(other.OnDataReceivedDelegate)),
      OnCompleteDelegate(std::move(other.OnCompleteDelegate))
{
}

HTTPResponse::~HTTPResponse()
{
}

void HTTPResponse::OnDataReceived(Span<char> data)
{
    HYP_SCOPE;

    { // lock mutex
        Mutex::Guard guard(m_mutex);

        const SizeType offset = m_body.Size();
        m_body.SetSize(m_body.Size() + data.Size());
        m_body.Write(data.Size(), offset, data.Data());
    }

    OnDataReceivedDelegate(data);
}

void HTTPResponse::OnComplete(int status_code)
{
    HYP_SCOPE;
    
    m_status_code = status_code;

    OnCompleteDelegate(m_status_code);
}

Optional<json::JSONValue> HTTPResponse::ToJSON() const
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    if (m_body.Empty()) {
        return { };
    }

    const json::ParseResult parse_result = json::JSON::Parse(String(m_body.ToByteView()));

    if (!parse_result.ok) {
        return { };
    }

    return parse_result.value;
}

#pragma endregion HTTPResponse

#pragma region HTTPRequest

HTTPRequest::HTTPRequest(const String &url, HTTPMethod method)
    : m_url(url),
      m_method(method)
{
}

HTTPRequest::HTTPRequest(const String &url, const json::JSONValue &body, HTTPMethod method)
    : m_url(url),
      m_method(method),
      m_content_type("application/json")
{
    const String body_string = body.ToString(true);
    m_body = ByteBuffer(body_string.Size(), body_string.Data());
}

HTTPRequest::HTTPRequest(const HTTPRequest &other)
    : m_url(other.m_url),
      m_method(other.m_method),
      m_body(other.m_body),
      m_content_type(other.m_content_type)
{
}

HTTPRequest &HTTPRequest::operator=(const HTTPRequest &other)
{
    if (this != &other) {
        m_url = other.m_url;
        m_method = other.m_method;
        m_body = other.m_body;
        m_content_type = other.m_content_type;
    }

    return *this;
}

HTTPRequest::HTTPRequest(HTTPRequest &&other) noexcept
    : m_url(std::move(other.m_url)),
      m_method(other.m_method),
      m_body(std::move(other.m_body)),
      m_content_type(std::move(other.m_content_type))
{
}

HTTPRequest &HTTPRequest::operator=(HTTPRequest &&other) noexcept
{
    if (this != &other) {
        m_url = std::move(other.m_url);
        m_method = other.m_method;
        m_body = std::move(other.m_body);
        m_content_type = std::move(other.m_content_type);
    }

    return *this;
}

HTTPRequest::~HTTPRequest()
{
}

Task<HTTPResponse> HTTPRequest::Send()
{
    HYP_SCOPE;
    
    return g_engine->GetNetRequestThread()->GetScheduler().Enqueue([url = m_url, content_type = m_content_type, body = m_body, method = m_method]() -> HTTPResponse
    {
        HYP_SCOPE;
    
        HTTPResponse response;

#if defined(HYP_CURL) && HYP_CURL
        CURL *curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.Data());

            // Set the request body
            if (!body.Empty()) {
                HYP_LOG(Net, LogLevel::DEBUG, "Sending body: {}", String(body.ToByteView()));
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.Data());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.Size());
            }

            // set headers
            struct curl_slist *headers = nullptr;

            if (!content_type.Empty()) {
                headers = curl_slist_append(headers, HYP_FORMAT("Content-Type: {}", content_type).Data());
            }

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            switch (method) {
            case HTTPMethod::GET:
                curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
                break;
            case HTTPMethod::POST:
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                break;
            case HTTPMethod::PUT:
                curl_easy_setopt(curl, CURLOPT_PUT, 1L);
                break;
            case HTTPMethod::PATCH:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
                break;
            case HTTPMethod::DELETE_:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
                break;
            default:
                break;
            }

            // Set the write callback function
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

            // Set the response object as the user data
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            // Perform the request
            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                HYP_LOG(Net, LogLevel::ERR, "curl_easy_perform() failed: {}", curl_easy_strerror(res));
            }

            curl_easy_cleanup(curl);

            response.OnComplete(res);

            return response;

        } else {
            HYP_LOG(Net, LogLevel::ERR, "curl_easy_init() failed");

            response.OnComplete(-1);

            return response;
        }
#else
        HYP_LOG(Net, LogLevel::ERR, "No HTTP request implementation available");

        response.OnComplete(-1);

        return response;
#endif
    });
}

#pragma endregion HTTPRequest

} // namespace net
} // namespace hyperion