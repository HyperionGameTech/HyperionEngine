#include <core/net/Socket.hpp>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>

namespace hyperion::v2 {

struct SocketServerImpl
{
    int socket_id = 0;
    struct sockaddr_un local, remote;
};

SocketServer::SocketServer()
    : m_impl(nullptr)
{
}

SocketServer::~SocketServer()
{
    if (m_impl != nullptr) {
        // TODO
    }
}

void SocketConnection::TriggerProc(Name event_name, Array<SocketProcArgument> &&args)
{
    const auto it = m_event_procs.Find(event_name);

    if (it == m_event_procs.End()) {
        return;
    }

    it->value(std::move(args));
}

bool SocketServer::Start()
{
    if (m_impl) {
        // TODO close
        
    }

    m_impl.Reset(new SocketServerImpl);

    m_impl->socket_id = socket(AF_UNIX, SOCK_STREAM, 0);

    if (m_impl->socket_id == -1) {
        DebugLog(
            LogType::Error,
            "Failed to open socket server. Code: %d\n",
            errno
        );

        TriggerProc(HYP_NAME(OnError), { SocketProcArgument(String("Failed to open socket")), SocketProcArgument(Int32(errno)) });

        return false;
    }

    constexpr const char *socket_path = "hyp_server.sock";
    constexpr SizeType max_connections = 5;

    m_impl->local.sun_family = AF_UNIX;
    strcpy(m_impl->local.sun_path, socket_path);
    unlink(m_impl->local.sun_path);

    const Int len = strlen(m_impl->local.sun_path) + sizeof(m_impl->local.sun_family);

    Int32 reuse_socket = 1;

    if (setsockopt(m_impl->socket_id, SOL_SOCKET, SO_REUSEADDR, &reuse_socket, sizeof(reuse_socket)) != 0) {
        const Int32 error_code = errno;

        TriggerProc(HYP_NAME(OnError), { SocketProcArgument(String(strerror(error_code))), SocketProcArgument(error_code) });

        return false;
    }

    if (setsockopt(m_impl->socket_id, SOL_SOCKET, SO_REUSEPORT, &reuse_socket, sizeof(reuse_socket)) != 0) {
        const Int32 error_code = errno;

        TriggerProc(HYP_NAME(OnError), { SocketProcArgument(String(strerror(error_code))), SocketProcArgument(error_code) });

        return false;
    }

    if (bind(m_impl->socket_id, (struct sockaddr *)&m_impl->local, len) != 0) {
        const Int32 error_code = errno;

        TriggerProc(HYP_NAME(OnError), { SocketProcArgument(String(strerror(error_code))), SocketProcArgument(error_code) });


        return false;
    }

    if (listen(m_impl->socket_id, max_connections) != 0) {
        const Int32 error_code = errno;

        TriggerProc(HYP_NAME(OnError), { SocketProcArgument(String(strerror(error_code))), SocketProcArgument(Int32(errno)) });

        return false;
    }

    TriggerProc(HYP_NAME(OnServerStarted), { });

    return true;
}

} // namespace hyperion::v2