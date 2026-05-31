/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/net/HTTPRequest.hpp>
#include <Core/net/NetRequestThread.hpp>

#include <Core/threading/TaskSystem.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <Core/profiling/ProfileScope.hpp>

#if defined(HYP_CURL) && HYP_CURL
#include <curl/curl.h>
#endif

namespace Hyperion {
namespace net {

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    HTTPResponse* response = static_cast<HTTPResponse*>(userp);
    const size_t realsize = size * nmemb;

    response->OnDataReceived(Span<char>(static_cast<char*>(contents), realsize));

    return realsize;
}

#pragma region HTTPResponse

HTTPResponse::HTTPResponse()
    : m_statusCode(0)
{
}

HTTPResponse::HTTPResponse(HTTPResponse&& other) noexcept
    : m_body(std::move(other.m_body)),
      m_statusCode(other.m_statusCode),
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

        const size_t offset = m_body.Size();
        m_body.SetSize(m_body.Size() + data.Size());
        m_body.Write(data.Size(), offset, data.Data());
    }

    OnDataReceivedDelegate(data);
}

void HTTPResponse::OnComplete(int statusCode)
{
    HYP_SCOPE;

    m_statusCode = statusCode;

    OnCompleteDelegate(m_statusCode);
}

Optional<JSON::Value> HTTPResponse::ToJSON() const
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mutex);

    if (m_body.Empty())
    {
        return {};
    }

    const JSON::ParseResult parseResult = JSON::Parse(String(m_body.ToByteView()));

    if (!parseResult.ok)
    {
        return {};
    }

    return parseResult.value;
}

#pragma endregion HTTPResponse

#pragma region HTTPRequest

HTTPRequest::HTTPRequest(const String& url, HTTPMethod method)
    : m_url(url),
      m_method(method)
{
}

HTTPRequest::HTTPRequest(const String& url, const JSON::Value& body, HTTPMethod method)
    : m_url(url),
      m_method(method),
      m_contentType("application/json")
{
    const String bodyString = body.ToString(true);
    m_body = ByteBuffer(bodyString.Size(), bodyString.Data());
}

HTTPRequest::HTTPRequest(const HTTPRequest& other)
    : m_url(other.m_url),
      m_method(other.m_method),
      m_body(other.m_body),
      m_contentType(other.m_contentType)
{
}

HTTPRequest& HTTPRequest::operator=(const HTTPRequest& other)
{
    if (this != &other)
    {
        m_url = other.m_url;
        m_method = other.m_method;
        m_body = other.m_body;
        m_contentType = other.m_contentType;
    }

    return *this;
}

HTTPRequest::HTTPRequest(HTTPRequest&& other) noexcept
    : m_url(std::move(other.m_url)),
      m_method(other.m_method),
      m_body(std::move(other.m_body)),
      m_contentType(std::move(other.m_contentType))
{
}

HTTPRequest& HTTPRequest::operator=(HTTPRequest&& other) noexcept
{
    if (this != &other)
    {
        m_url = std::move(other.m_url);
        m_method = other.m_method;
        m_body = std::move(other.m_body);
        m_contentType = std::move(other.m_contentType);
    }

    return *this;
}

HTTPRequest::~HTTPRequest()
{
}

Task<HTTPResponse> HTTPRequest::Send()
{
    HYP_SCOPE;

    RC<NetRequestThread> netRequestThread = GetGlobalNetRequestThread();

    if (!netRequestThread)
    {
        HYP_LOG(Net, Error, "No global NetRequestThread set!");

        return Task<HTTPResponse>();
    }

    return netRequestThread->GetScheduler().Enqueue([url = m_url, contentType = m_contentType, body = m_body, method = m_method]() -> HTTPResponse
        {
            HYP_SCOPE;

            HTTPResponse response;

#if defined(HYP_CURL) && HYP_CURL
            CURL* curl = curl_easy_init();
            if (curl)
            {
                curl_easy_setopt(curl, CURLOPT_URL, url.Data());

                // Set the request body
                if (!body.Empty())
                {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.Data());
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.Size());
                }

                // set headers
                struct curl_slist* headers = nullptr;

                if (!contentType.Empty())
                {
                    headers = curl_slist_append(headers, HYP_FORMAT("Content-Type: {}", contentType).Data());
                }

                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

                switch (method)
                {
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
                if (res != CURLE_OK)
                {
                    HYP_LOG(Net, Error, "curl_easy_perform() failed: {}", curl_easy_strerror(res));
                }

                // Get the response code
                long responseCode = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

                curl_easy_cleanup(curl);

                response.OnComplete(int(responseCode));

                return response;
            }
            else
            {
                HYP_LOG(Net, Error, "curl_easy_init() failed");

                response.OnComplete(-1);

                return response;
            }
#else
            HYP_LOG(Net, Error, "No HTTP request implementation available");

            response.OnComplete(-1);

            return response;
#endif
        });
}

#pragma endregion HTTPRequest

} // namespace net
} // namespace Hyperion