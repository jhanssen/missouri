#include "UdpServer.h"
#include <Winsock2.h>
#include <vector>
#include <pthread.h>
#include <stdio.h>

class UdpServerPrivate
{
public:
    SOCKET server;
    std::vector<SOCKET> clients;

    pthread_t thread;
    pthread_mutex_t mutex;

    static void* run(void* arg);
};

void* UdpServerPrivate::run(void* arg)
{
    sockaddr_in from;
    int fromlen;
    SOCKET socket;

    UdpServerPrivate* priv = static_cast<UdpServerPrivate*>(arg);

    for (;;) {
        fromlen = sizeof(from);
        socket = accept(priv->server, reinterpret_cast<sockaddr*>(&from), &fromlen);
        if (socket == INVALID_SOCKET) {
            fprintf(stderr, "accept returned invalid socket\n");
            return 0;
        }

        pthread_mutex_lock(&priv->mutex);
        priv->clients.push_back(socket);
        pthread_mutex_unlock(&priv->mutex);
    }

    return 0;
}

UdpServer::UdpServer(int port)
    : mPriv(new UdpServerPrivate)
{
    WSADATA data;
    WSAStartup(0x0202, &data);

    sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port);
    mPriv->server = socket(AF_INET, SOCK_DGRAM, 0);
    bind(mPriv->server, reinterpret_cast<sockaddr*>(&local), sizeof(local));
    listen(mPriv->server, 5);

    pthread_mutex_init(&mPriv->mutex, 0);
    pthread_create(&mPriv->thread, 0, UdpServerPrivate::run, mPriv);
}

UdpServer::~UdpServer()
{
    void* ret;
    pthread_join(mPriv->thread, &ret);
    pthread_mutex_destroy(&mPriv->mutex);

    closesocket(mPriv->server);

    std::vector<SOCKET>::const_iterator sock = mPriv->clients.begin();
    const std::vector<SOCKET>::const_iterator end = mPriv->clients.end();
    while (sock != end) {
        closesocket(*sock);
        ++sock;
    }

    delete mPriv;
    WSACleanup();
}

