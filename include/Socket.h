#pragma once
#include<iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include "noncopyable.h"
#include "InetAddress.h"

class InetAddress;

// 独占 socket fd，析构时关闭；提供监听、接入和写端半关闭接口。
class Socket: noncopyable
{
    public:
    explicit Socket(int sockfd);
    ~Socket();

    int Getfd() const;
    void BindAddress(const InetAddress &currAddr);
    void listen();

    // 返回新连接 fd，并写入对端地址。
    int accept(InetAddress *clientAddr);

    void shutDownWrite();

    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

    private:
    const int sockfd;
};
