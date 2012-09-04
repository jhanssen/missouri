#include "GDICapturer.h"
#include "Encoder.h"
#include "Receiver.h"
#include "TcpServer.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include <Windows.h>
#include <Winsock2.h>
#include <stdio.h>
#include <assert.h>

class Connection
{
public:
    Connection(TcpSocket* socket, Encoder* encoder);

private:
    static bool dataReady(const char* data, int size, void* userData);
    static void closed(void* userData);

private:
    TcpSocket* mSocket;
    Encoder* mEncoder;
    Receiver mReceiver;
    Host mHost;
    bool mReffed;
};

Connection::Connection(TcpSocket* socket, Encoder* encoder)
    : mSocket(socket), mEncoder(encoder), mReffed(false)
{
    mSocket->setDataCallback(dataReady, this);
    mSocket->setCloseCallback(closed, this);
    mHost = mSocket->remoteHost();
}

void Connection::closed(void* userData)
{
    Connection* conn = static_cast<Connection*>(userData);
    conn->mEncoder->deref(conn->mHost);
    delete conn->mSocket;
    delete conn;
}

bool Connection::dataReady(const char* data, int size, void* userData)
{
    Connection* conn = static_cast<Connection*>(userData);
    conn->mReceiver.feed(data, size);
    char* dt;
    int sz;
    for (;;) {
        if (!conn->mReceiver.popBlock(&dt, &sz))
            break;
        if (sz == 8 && !conn->mReffed) {
            conn->mReffed = true;

            const int w = ntohl(*reinterpret_cast<uint32_t*>(dt));
            const int h = ntohl(*reinterpret_cast<uint32_t*>(dt + 4));
            conn->mEncoder->ref(conn->mHost, w, h);

            uint8_t* payload;
            int32_t size, tmp;

            conn->mEncoder->getSps(&payload, &size);
            tmp = htonl(size);
            conn->mSocket->send(reinterpret_cast<char*>(&tmp), 4);
            conn->mSocket->send(reinterpret_cast<char*>(payload), size);

            conn->mEncoder->getPps(&payload, &size);
            tmp = htonl(size);
            conn->mSocket->send(reinterpret_cast<char*>(&tmp), 4);
            conn->mSocket->send(reinterpret_cast<char*>(payload), size);
        }
        free(dt);
    }
    return true;
}

static bool newSocket(TcpSocket* socket, void* userData)
{
    Encoder* enc = static_cast<Encoder*>(userData);
    printf("got connection, encoder %p\n", enc);

    (void)new Connection(socket, enc);
}

int main(int argc, char** argv)
{
    WSADATA data;
    const int ret = WSAStartup(0x0202, &data);
    if (ret) {
        fprintf(stderr, "WSAStartup failed: %d %s\n", ret, UdpSocket::socketErrorMessage(ret).c_str());
        return 1;
    }

    GDICapturer cap;
    const uint8_t* buffer = cap.GetBuffer();
    const int32_t width = cap.GetWidth();
    const int32_t height = cap.GetHeight();
    const int32_t size = cap.GetImageSize();
    Encoder enc(buffer, width, height, size);

    const uint16_t port = 21047;
    TcpServer server;
    server.setCallback(newSocket, &enc);
    if (!server.listen(port)) {
        fprintf(stderr, "Server listen failed\n");
        WSACleanup();
        return 1;
    }
    fprintf(stdout, "Server listening on port %u\n", port);

    const int sleepFor = 1000 / 60;
    for (;;) {
        cap.Capture();
        enc.encode();
        Sleep(sleepFor);
    }

    WSACleanup();
    return 0;
}
