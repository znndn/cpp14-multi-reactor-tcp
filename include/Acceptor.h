#pragma once

#include <functional>

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

class EventLoop;
class InetAddress;

// 在 baseLoop 上接收新连接，将已接受的 fd 交给 TcpServer。
class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb)
    {
        newConnectionCallback_ = cb;
    }

    bool listenning() const
    {
        return listenning_;
    }

    void listen();

private:
    void handleRead();

    // 监听 Channel 与连接接入均归属 baseLoop。
    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};
