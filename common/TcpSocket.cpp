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

class TcpSocketPrivate : public Thread
{
public:
    bool connected, stopped;

    SOCKET client;

    Mutex mutex;

    TcpSocket::DataCallbackFunc dataCallback;
    void* dataUserData;
    TcpSocket::CloseCallbackFunc closeCallback;
    void* closeUserData;

    uint8_t* pending;
    int pendingSize;

    virtual void run();
};

void TcpSocketPrivate::run()
{
    int ret;
    timeval tv;
    fd_set fds;
    char buf[4096];

    for (;;) {
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        FD_ZERO(&fds);
        FD_SET(client, &fds);

        ret = select(client + 1, &fds, 0, 0, &tv);
        if (ret == SOCKET_ERROR) {
            const int err = socketError();
            fprintf(stderr, "TcpSocket select failed: %d %s\n", err, socketErrorMessage(err).c_str());
            return;
        } else if (ret > 0) {
            assert(FD_ISSET(client, &fds));
            bool done;
            do {
                done = true;
                ret = recv(client, buf, sizeof(buf), 0);
                if (ret == 0) {
                    // connection closed
                    printf("TcpSocket connection closed\n");
                    MutexLocker locker(&mutex);
                    if (closeCallback)
                        closeCallback(closeUserData);
                    return;
                } else if (ret == SOCKET_ERROR) {
                    const int err = socketError();
                    if (err == EINTR) {
                        done = false;
                    }
#ifdef OS_Windows
                    else if (err == WSAECONNRESET) {
                        if (closeCallback)
                            closeCallback(closeUserData);
                        return;
                    }
#endif
                    else {
                        fprintf(stderr, "TcpSocket recv error: %d %s\n", err, socketErrorMessage(err).c_str());
                        return;
                    }
                }
            } while (!done);
            //printf("TcpSocket got socket data %d\n", ret);
            MutexLocker locker(&mutex);
            if (dataCallback) {
                if (!dataCallback(buf, ret, dataUserData))
                    return;
            } else {
                if (!pending) {
                    pending = reinterpret_cast<uint8_t*>(malloc(ret));
                    memcpy(pending, buf, ret);
                    pendingSize = ret;
                } else {
                    pending = reinterpret_cast<uint8_t*>(realloc(pending, pendingSize + ret));
                    memcpy(pending + pendingSize, buf, ret);
                    pendingSize += ret;
                }
            }
        }
        //printf("tcp client wakeup\n");

        MutexLocker locker(&mutex);
        if (stopped)
            return;
    }
}

TcpSocket::TcpSocket()
    : mPriv(new TcpSocketPrivate)
{
    mPriv->stopped = false;
    mPriv->connected = false;
    mPriv->dataCallback = 0;
    mPriv->closeCallback = 0;
    mPriv->pending = 0;
    mPriv->pendingSize = 0;
}

TcpSocket::~TcpSocket()
{
    if (mPriv->connected) {
        void* ret;

        MutexLocker locker(&mPriv->mutex);
        mPriv->stopped = true;
        locker.unlock();
        mPriv->join();

        close(mPriv->client);
    }
    if (mPriv->pending)
        free(mPriv->pending);

    delete mPriv;
}

void TcpSocket::setDataCallback(DataCallbackFunc callback, void* userData)
{
    MutexLocker locker(&mPriv->mutex);
    mPriv->dataCallback = callback;
    mPriv->dataUserData = userData;
    if (mPriv->pending) {
        callback(reinterpret_cast<char*>(mPriv->pending), mPriv->pendingSize, userData);
        free(mPriv->pending);
        mPriv->pending = 0;
        mPriv->pendingSize = 0;
    }
}

void TcpSocket::setCloseCallback(CloseCallbackFunc callback, void* userData)
{
    MutexLocker locker(&mPriv->mutex);
    mPriv->closeCallback = callback;
    mPriv->closeUserData = userData;
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

    mPriv->start();
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
    //printf("TcpSocket sent %ld\n", total);

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

    mPriv->start();
    mPriv->connected = true;
}

Host TcpSocket::remoteHost() const
{
    if (!mPriv->connected)
        return Host();
    sockaddr_in addr;
    socklen_t addrsz = sizeof(sockaddr_in);
    getpeername(mPriv->client, reinterpret_cast<sockaddr*>(&addr), &addrsz);
    return Host(addr.sin_addr.s_addr, ntohs(addr.sin_port));
}
