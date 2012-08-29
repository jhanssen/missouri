#include "UdpServer.h"
#include <Winsock2.h>
#include <string>
#include <pthread.h>
#include <stdio.h>

static inline std::string wsaErrorMessage(int error)
{
    LPSTR errString = NULL;
    (void)FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, error, 0, (LPSTR)&errString, 0, 0);
    std::string str(errString);
    LocalFree(errString);
    return str;
}

class UdpServerPrivate
{
public:
    bool listening, stopped;

    SOCKET server;

    pthread_t thread;
    pthread_mutex_t mutex;

    static void* run(void* arg);
};

void* UdpServerPrivate::run(void* arg)
{
    sockaddr_in from;
    int fromlen, ret;
    timeval tv;
    fd_set fds;
    char buf[4096];

    UdpServerPrivate* priv = static_cast<UdpServerPrivate*>(arg);

    for (;;) {
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        FD_ZERO(&fds);
        FD_SET(priv->server, &fds);

        ret = select(-1, &fds, 0, 0, &tv);
        if (ret == SOCKET_ERROR) {
            const int err = WSAGetLastError();
            fprintf(stderr, "socket failed: %d %s\n", err, wsaErrorMessage(err).c_str());
            return 0;
        } else if (ret > 0) {
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

UdpServer::UdpServer(int port)
    : mPriv(new UdpServerPrivate)
{
    mPriv->listening = false;
    mPriv->stopped = false;
    int ret;

    WSADATA data;
    ret = WSAStartup(0x0202, &data);
    if (ret) {
        fprintf(stderr, "WSAStartup failed: %d %s\n", ret, wsaErrorMessage(ret).c_str());
        return;
    }

    sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port);
    mPriv->server = socket(AF_INET, SOCK_DGRAM, 0);
    if (mPriv->server == INVALID_SOCKET) {
        const int err = WSAGetLastError();
        fprintf(stderr, "socket failed: %d %s\n", err, wsaErrorMessage(err).c_str());
        WSACleanup();
        return;
    }
    if (bind(mPriv->server, reinterpret_cast<sockaddr*>(&local), sizeof(local))) {
        const int err = WSAGetLastError();
        fprintf(stderr, "bind on port %d failed: %d %s\n", port, err, wsaErrorMessage(err).c_str());
        closesocket(mPriv->server);
        WSACleanup();
        return;
    }

    pthread_mutex_init(&mPriv->mutex, 0);
    pthread_create(&mPriv->thread, 0, UdpServerPrivate::run, mPriv);

    mPriv->listening = true;
}

UdpServer::~UdpServer()
{
    if (mPriv->listening) {
        void* ret;
        pthread_mutex_lock(&mPriv->mutex);
        mPriv->stopped = true;
        pthread_mutex_unlock(&mPriv->mutex);
        pthread_join(mPriv->thread, &ret);
        pthread_mutex_destroy(&mPriv->mutex);

        closesocket(mPriv->server);

        WSACleanup();
    }

    delete mPriv;
}

bool UdpServer::isListening() const
{
    return mPriv->listening;
}
