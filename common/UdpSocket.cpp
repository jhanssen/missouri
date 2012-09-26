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
#include "Util.h"
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

std::string UdpSocket::socketErrorMessage(int error)
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

class UdpSocketPrivate : public Thread
{
public:
    bool listening, stopped;

    SOCKET server;
    SOCKET client;

    sockaddr_in to;

    Mutex mutex;

    UdpSocket::CallbackFunc callback;
    void* userData;

    virtual void run();
};

void UdpSocketPrivate::run()
{
    sockaddr_in from;
    socklen_t fromlen;
    int ret;
    timeval tv;
    fd_set fds;
    char buf[4096];

    for (;;) {
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        FD_ZERO(&fds);
        FD_SET(server, &fds);

        ret = select(server + 1, &fds, 0, 0, &tv);
        if (ret == SOCKET_ERROR) {
            const int err = socketError();
            fprintf(stderr, "socket failed: %d %s\n", err, UdpSocket::socketErrorMessage(err).c_str());
            return;
        } else if (ret > 0) {
            assert(FD_ISSET(server, &fds));
            bool done;
            do {
                done = true;
                fromlen = sizeof(from);
                ret = recvfrom(server, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromlen);
                if (ret == SOCKET_ERROR) {
                    const int err = socketError();
                    if (err == EINTR)
                        done = false;
                    else {
                        fprintf(stderr, "socket recvfrom failed: %d %s\n", err, UdpSocket::socketErrorMessage(err).c_str());
                        return;
                    }
                }
                //printf("got socket data %d\n", ret);
                if (callback && !callback(buf, ret, userData)) {
                    return;
                }
            } while (!done);
        }
        //printf("server wakeup\n");

        MutexLocker locker(&mutex);
        if (stopped)
            return;
    }
}

UdpSocket::UdpSocket()
    : mPriv(new UdpSocketPrivate)
{
    memset(&mPriv->to, 0, sizeof(sockaddr_in));
    mPriv->listening = false;
    mPriv->stopped = false;
    mPriv->callback = 0;
}

UdpSocket::~UdpSocket()
{
    if (mPriv->listening) {
        void* ret;
        MutexLocker locker(&mPriv->mutex);
        mPriv->stopped = true;
        locker.unlock();
        mPriv->join();

        close(mPriv->server);
    }
    if (mPriv->to.sin_addr.s_addr) {
        close(mPriv->client);
    }

    delete mPriv;
}

void UdpSocket::setCallback(CallbackFunc callback, void* userData)
{
    mPriv->callback = callback;
    mPriv->userData = userData;
}

bool UdpSocket::listen(uint16_t port)
{
    if (mPriv->listening)
        return false;

    sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port);
    mPriv->server = socket(AF_INET, SOCK_DGRAM, 0);
    if (mPriv->server == INVALID_SOCKET) {
        const int err = socketError();
        fprintf(stderr, "socket failed: %d %s\n", err, socketErrorMessage(err).c_str());
        return false;
    }
    if (bind(mPriv->server, reinterpret_cast<sockaddr*>(&local), sizeof(local))) {
        const int err = socketError();
        fprintf(stderr, "bind on port %d failed: %d %s\n", port, err, socketErrorMessage(err).c_str());
        close(mPriv->server);
        return false;
    }

    mPriv->start();
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
            fprintf(stderr, "sendto failed in send: %d %s (addr %u %u)\n", err, socketErrorMessage(err).c_str(), addr, host.port());

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
