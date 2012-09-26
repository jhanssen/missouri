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
#include "TcpServer.h"
#include "TcpSocket.h"
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

class TcpServerPrivate : public Thread
{
public:
    bool listening, stopped;

    SOCKET server;

    TcpServer::CallbackFunc callback;
    void* userData;

    virtual void run();

    static Mutex mutex;
};

Mutex TcpServerPrivate::mutex;

void TcpServerPrivate::run()
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
            fprintf(stderr, "TcpServer socket failed: %d %s\n", err, socketErrorMessage(err).c_str());
            return;
        } else if (ret > 0) {
            assert(FD_ISSET(server, &fds));
            bool done;
            do {
                done = true;
                fromlen = sizeof(from);
                SOCKET sock = accept(server, reinterpret_cast<sockaddr*>(&from), &fromlen);
                if (sock == INVALID_SOCKET) {
                    const int err = socketError();
                    if (err == EINTR)
                        done = false;
                    else {
                        fprintf(stderr, "TcpServer socket accept failed: %d %s\n", err, socketErrorMessage(err).c_str());
                        return;
                    }
                }
                //printf("TcpServer got new socket connection\n");
                TcpSocket* socket = new TcpSocket;
                socket->setSocketDescriptor(reinterpret_cast<void*>(&sock));
                if (callback && !callback(socket, userData)) {
                    return;
                }
            } while (!done);
        }
        //printf("TcpServer server wakeup\n");

        MutexLocker locker(&mutex);
        if (stopped)
            return;
    }
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
        MutexLocker locker(&mPriv->mutex);
        mPriv->stopped = true;
        locker.unlock();
        mPriv->join();

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

    mPriv->start();
    mPriv->listening = true;

    return true;
}

bool TcpServer::isListening() const
{
    return mPriv->listening;
}
