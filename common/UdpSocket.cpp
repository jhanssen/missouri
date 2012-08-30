#include "UdpSocket.h"
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
#include <string>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>

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

static inline int socketError()
{
#ifdef OS_Windows
    return WSAGetLastError();
#else
    return errno;
#endif
}

#ifndef OS_Windows
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1

typedef int SOCKET;
#endif

class UdpSocketPrivate
{
public:
    bool listening, stopped;

    SOCKET server;
    SOCKET client;

    sockaddr_in to;

    pthread_t thread;
    pthread_mutex_t mutex;

    static void* run(void* arg);
};

void* UdpSocketPrivate::run(void* arg)
{
    sockaddr_in from;
    socklen_t fromlen;
    int ret;
    timeval tv;
    fd_set fds;
    char buf[4096];

    UdpSocketPrivate* priv = static_cast<UdpSocketPrivate*>(arg);

    for (;;) {
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        FD_ZERO(&fds);
        FD_SET(priv->server, &fds);

        ret = select(priv->server + 1, &fds, 0, 0, &tv);
        if (ret == SOCKET_ERROR) {
            const int err = socketError();
            fprintf(stderr, "socket failed: %d %s\n", err, socketErrorMessage(err).c_str());
            return 0;
        } else if (ret > 0) {
            assert(FD_ISSET(priv->server, &fds));
            fromlen = sizeof(from);
            ret = recvfrom(priv->server, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromlen);
            printf("got socket data %d\n", ret);
        }
        printf("server wakeup\n");

        pthread_mutex_lock(&priv->mutex);
        if (priv->stopped) {
            pthread_mutex_unlock(&priv->mutex);
            return 0;
        }
        pthread_mutex_unlock(&priv->mutex);
    }

    return 0;
}

UdpSocket::UdpSocket()
    : mPriv(new UdpSocketPrivate)
{
    memset(&mPriv->to, 0, sizeof(sockaddr_in));
    mPriv->listening = false;
    mPriv->stopped = false;
}

UdpSocket::~UdpSocket()
{
    if (mPriv->listening) {
        void* ret;
        pthread_mutex_lock(&mPriv->mutex);
        mPriv->stopped = true;
        pthread_mutex_unlock(&mPriv->mutex);
        pthread_join(mPriv->thread, &ret);
        pthread_mutex_destroy(&mPriv->mutex);

#ifdef OS_Windows
        closesocket(mPriv->server);

        WSACleanup();
#else
        close(mPriv->server);
#endif
    }
    if (mPriv->to.sin_addr.s_addr) {
        close(mPriv->client);
    }

    delete mPriv;
}

bool UdpSocket::listen(uint16_t port)
{
    if (mPriv->listening)
        return false;

#ifdef OS_Windows
    WSADATA data;
    const int ret = WSAStartup(0x0202, &data);
    if (ret) {
        fprintf(stderr, "WSAStartup failed: %d %s\n", ret, socketErrorMessage(ret).c_str());
        return false;
    }
#endif

    sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port);
    mPriv->server = socket(AF_INET, SOCK_DGRAM, 0);
    if (mPriv->server == INVALID_SOCKET) {
        const int err = socketError();
        fprintf(stderr, "socket failed: %d %s\n", err, socketErrorMessage(err).c_str());
#ifdef OS_Windows
        WSACleanup();
#endif
        return false;
    }
    if (bind(mPriv->server, reinterpret_cast<sockaddr*>(&local), sizeof(local))) {
        const int err = socketError();
        fprintf(stderr, "bind on port %d failed: %d %s\n", port, err, socketErrorMessage(err).c_str());
#ifdef OS_Windows
        closesocket(mPriv->server);
        WSACleanup();
#else
        close(mPriv->server);
#endif
        return false;
    }

    pthread_mutex_init(&mPriv->mutex, 0);
    pthread_create(&mPriv->thread, 0, UdpSocketPrivate::run, mPriv);

    mPriv->listening = true;

    return true;
}

bool UdpSocket::send(const Host& host, const char* data, int size)
{
    const uint32_t addr = host.address();
    const uint16_t port = htons(host.port());
    sockaddr_in& to = mPriv->to;

    if (to.sin_addr.s_addr != addr || to.sin_port != port) {
        if (to.sin_addr.s_addr)
            close(mPriv->client);
        to.sin_family = AF_INET;
        to.sin_addr.s_addr = addr;
        to.sin_port = port;
        mPriv->client = socket(AF_INET, SOCK_DGRAM, 0);
        if (mPriv->client == INVALID_SOCKET) {
            memset(&to, 0, sizeof(sockaddr_in));

            const int err = socketError();
            fprintf(stderr, "socket failed in send: %d %s\n", err, socketErrorMessage(err).c_str());

            return false;
        }
    }

    int err;
    ssize_t total = 0, sent;
    do {
        sent = sendto(mPriv->client, &data[total], size - total, 0, reinterpret_cast<sockaddr*>(&to), sizeof(sockaddr_in));
        if (sent == SOCKET_ERROR) {
            const int err = socketError();
            if (err == EINTR)
                continue;
            fprintf(stderr, "sendto failed in send: %d %s\n", err, socketErrorMessage(err).c_str());

            close(mPriv->client);
            memset(&to, 0, sizeof(sockaddr_in));
            return false;
        }
        total += sent;
    } while (total < size);

    assert(total == size);
    return true;
}

bool UdpSocket::isListening() const
{
    return mPriv->listening;
}
