#include "TcpServer.h"
#include "TcpSocket.h"
#ifdef OS_Windows
# include <Winsock2.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/select.h>
# include <netinet/in.h>
# include <string.h>
# include <errno.h>
# include <unistd.h>
#endif
#include <pthread.h>
#include <stdio.h>
#include <assert.h>

static inline int socketError()
{
#ifdef OS_Windows
    return WSAGetLastError();
#else
    return errno;
#endif
}

static inline std::string socketErrorMessage(int error)
{
#ifdef OS_Windows
    LPSTR errString = NULL;
    (void)FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, error, 0, (LPSTR)&errString, 0, 0);
    std::string str(errString);
    LocalFree(errString);
    return str;
#else
    static const size_t buflen = 1024;
    static char buf[buflen];
    (void)strerror_r(error, buf, buflen);
    return std::string(buf);
#endif
}

#ifdef OS_Windows
typedef int socklen_t;

static inline void close(SOCKET socket)
{
    closesocket(socket);
}
#else
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1

typedef int SOCKET;
#endif

class TcpServerPrivate
{
public:
    bool listening, stopped;

    SOCKET server;

    pthread_t thread;
    pthread_mutex_t mutex;

    TcpServer::CallbackFunc callback;
    void* userData;

    static void* run(void* arg);
};

void* TcpServerPrivate::run(void* arg)
{
    sockaddr_in from;
    socklen_t fromlen;
    int ret;
    timeval tv;
    fd_set fds;
    char buf[4096];

    TcpServerPrivate* priv = static_cast<TcpServerPrivate*>(arg);

    for (;;) {
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        FD_ZERO(&fds);
        FD_SET(priv->server, &fds);

        ret = select(priv->server + 1, &fds, 0, 0, &tv);
        if (ret == SOCKET_ERROR) {
            const int err = socketError();
            fprintf(stderr, "TcpServer socket failed: %d %s\n", err, socketErrorMessage(err).c_str());
            return 0;
        } else if (ret > 0) {
            assert(FD_ISSET(priv->server, &fds));
            bool done;
            do {
                done = true;
                fromlen = sizeof(from);
                SOCKET sock = accept(priv->server, reinterpret_cast<sockaddr*>(&from), &fromlen);
                if (sock == INVALID_SOCKET) {
                    const int err = socketError();
                    if (err == EINTR)
                        done = false;
                    else {
                        fprintf(stderr, "TcpServer socket accept failed: %d %s\n", err, socketErrorMessage(err).c_str());
                        return 0;
                    }
                }
                //printf("TcpServer got new socket connection\n");
                TcpSocket* socket = new TcpSocket;
                socket->setSocketDescriptor(reinterpret_cast<void*>(&sock));
                if (priv->callback && !priv->callback(socket, priv->userData)) {
                    return 0;
                }
            } while (!done);
        }
        //printf("TcpServer server wakeup\n");

        pthread_mutex_lock(&priv->mutex);
        if (priv->stopped) {
            pthread_mutex_unlock(&priv->mutex);
            return 0;
        }
        pthread_mutex_unlock(&priv->mutex);
    }

    return 0;
}

TcpServer::TcpServer()
    : mPriv(new TcpServerPrivate)
{
    mPriv->listening = false;
    mPriv->stopped = false;
    mPriv->callback = 0;
}

TcpServer::~TcpServer()
{
    if (mPriv->listening) {
        void* ret;
        pthread_mutex_lock(&mPriv->mutex);
        mPriv->stopped = true;
        pthread_mutex_unlock(&mPriv->mutex);
        pthread_join(mPriv->thread, &ret);
        pthread_mutex_destroy(&mPriv->mutex);

        close(mPriv->server);
    }

    delete mPriv;
}

void TcpServer::setCallback(CallbackFunc callback, void* userData)
{
    mPriv->callback = callback;
    mPriv->userData = userData;
}

bool TcpServer::listen(uint16_t port)
{
    if (mPriv->listening)
        return false;

    sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port);
    mPriv->server = socket(AF_INET, SOCK_STREAM, 0);
    if (mPriv->server == INVALID_SOCKET) {
        const int err = socketError();
        fprintf(stderr, "TcpServer socket failed: %d %s\n", err, socketErrorMessage(err).c_str());
        return false;
    }
    if (bind(mPriv->server, reinterpret_cast<sockaddr*>(&local), sizeof(local))) {
        const int err = socketError();
        fprintf(stderr, "TcpServer bind on port %d failed: %d %s\n", port, err, socketErrorMessage(err).c_str());
        close(mPriv->server);
        return false;
    }
    if (::listen(mPriv->server, 5)) {
        const int err = socketError();
        fprintf(stderr, "TcpServer listen on port %d failed: %d %s\n", port, err, socketErrorMessage(err).c_str());
        close(mPriv->server);
        return false;
    }

    pthread_mutex_init(&mPriv->mutex, 0);
    pthread_create(&mPriv->thread, 0, TcpServerPrivate::run, mPriv);

    mPriv->listening = true;

    return true;
}

bool TcpServer::isListening() const
{
    return mPriv->listening;
}
