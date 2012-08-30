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

class TcpSocketPrivate
{
public:
    bool connected, stopped;

    SOCKET client;

    pthread_t thread;
    pthread_mutex_t mutex;

    TcpSocket::CallbackFunc callback;
    void* userData;

    static void* run(void* arg);
};

void* TcpSocketPrivate::run(void* arg)
{
    int ret;
    timeval tv;
    fd_set fds;
    char buf[4096];

    TcpSocketPrivate* priv = static_cast<TcpSocketPrivate*>(arg);

    for (;;) {
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        FD_ZERO(&fds);
        FD_SET(priv->client, &fds);

        ret = select(priv->client + 1, &fds, 0, 0, &tv);
        if (ret == SOCKET_ERROR) {
            const int err = socketError();
            fprintf(stderr, "TcpSocket select failed: %d %s\n", err, socketErrorMessage(err).c_str());
            return 0;
        } else if (ret > 0) {
            assert(FD_ISSET(priv->client, &fds));
            bool done;
            do {
                done = true;
                ret = recv(priv->client, buf, sizeof(buf), 0);
                if (ret == 0) {
                    // connection closed
#warning remember to handle connection closed here
                    printf("TcpSocket connection closed\n");
                    return 0;
                } else if (ret == SOCKET_ERROR) {
                    const int err = socketError();
                    if (err == EINTR)
                        done = false;
                    else {
                        fprintf(stderr, "TcpSocket recv error: %d %s\n", err, socketErrorMessage(err).c_str());
                        return 0;
                    }
                }
            } while (!done);
            printf("TcpSocket got socket data %d\n", ret);
            if (priv->callback && !priv->callback(buf, ret, priv->userData)) {
                return 0;
            }
        }
        printf("tcp client wakeup\n");

        pthread_mutex_lock(&priv->mutex);
        if (priv->stopped) {
            pthread_mutex_unlock(&priv->mutex);
            return 0;
        }
        pthread_mutex_unlock(&priv->mutex);
    }

    return 0;
}

TcpSocket::TcpSocket()
    : mPriv(new TcpSocketPrivate)
{
    mPriv->stopped = false;
    mPriv->connected = false;
    mPriv->callback = 0;
}

TcpSocket::~TcpSocket()
{
    if (mPriv->connected) {
        void* ret;

        pthread_mutex_lock(&mPriv->mutex);
        mPriv->stopped = true;
        pthread_mutex_unlock(&mPriv->mutex);
        pthread_join(mPriv->thread, &ret);
        pthread_mutex_destroy(&mPriv->mutex);

        close(mPriv->client);
    }

    delete mPriv;
}

void TcpSocket::setCallback(CallbackFunc callback, void* userData)
{
    mPriv->callback = callback;
    mPriv->userData = userData;
}

bool TcpSocket::connect(const Host& host)
{
    if (mPriv->connected)
        return false;

    sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = host.address();
    local.sin_port = htons(host.port());
    mPriv->client = socket(AF_INET, SOCK_STREAM, 0);
    if (mPriv->client == INVALID_SOCKET) {
        const int err = socketError();
        fprintf(stderr, "TcpSocket socket failed: %d %s\n", err, socketErrorMessage(err).c_str());
        return false;
    }
    if (::connect(mPriv->client, reinterpret_cast<sockaddr*>(&local), sizeof(local))) {
        const int err = socketError();
        fprintf(stderr, "TcpSocket connect to %u:%u failed: %d %s\n", host.address(), host.port(), err, socketErrorMessage(err).c_str());
        close(mPriv->client);
        return false;
    }

    pthread_mutex_init(&mPriv->mutex, 0);
    pthread_create(&mPriv->thread, 0, TcpSocketPrivate::run, mPriv);

    mPriv->connected = true;

    return true;
}

bool TcpSocket::send(const char* data, int size)
{
    int err;
    ssize_t total = 0, sent;
    do {
        sent = ::send(mPriv->client, &data[total], size - total, 0);
        if (sent == SOCKET_ERROR) {
            const int err = socketError();
            if (err == EINTR)
                continue;
            fprintf(stderr, "TcpSocket send failed in send: %d %s\n", err, socketErrorMessage(err).c_str());

            close(mPriv->client);
            return false;
        }
        total += sent;
    } while (total < size);
        printf("TcpSocket sent %ld\n", total);

    assert(total == size);
    return true;
}

bool TcpSocket::isConnected() const
{
    return mPriv->connected;
}

void TcpSocket::setSocketDescriptor(void* socket)
{
    if (mPriv->connected)
        return;
    mPriv->client = *reinterpret_cast<SOCKET*>(socket);

    pthread_mutex_init(&mPriv->mutex, 0);
    pthread_create(&mPriv->thread, 0, TcpSocketPrivate::run, mPriv);

    mPriv->connected = true;
}
