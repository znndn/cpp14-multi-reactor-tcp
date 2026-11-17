#include<iostream>
#include <cstring>
#include"Socket.h"
#include "InetAddress.h"
#include "Logger.h"

Socket::Socket(int sockfd): sockfd(sockfd)
{
}

Socket::~Socket()
{
    ::close(sockfd);
}

int Socket::Getfd() const
{
    return sockfd;
}

void Socket::BindAddress(const InetAddress &currAddr)
{
    int result=::bind(sockfd,reinterpret_cast<const struct sockaddr*>(currAddr.getSocketAddress()),sizeof(struct sockaddr_in));
    if (result!=0)
    {
        log_fatal(" bind address %d fail",sockfd);
    }
}

void Socket::listen()
{
    // backlog 指待 accept 的连接队列长度，实际值受系统上限约束。
    int result=::listen(sockfd,4096);
    if (result==-1)
    {
        log_fatal("failed to listen to sockfd %d",sockfd);
    }
}

int Socket::accept(InetAddress *clientAddr)
{
    sockaddr_in tempSockInfo;
    socklen_t InfoLength=sizeof(tempSockInfo);
    memset(&tempSockInfo,0,sizeof(tempSockInfo));
    // 新连接直接设置为非阻塞，并在 exec 时关闭。
    int result=::accept4(sockfd,
        reinterpret_cast<struct sockaddr*>(&tempSockInfo),
        &InfoLength,SOCK_NONBLOCK|SOCK_CLOEXEC);
    if (result>=0)
    {
        clientAddr->setSocketAddress(tempSockInfo);
    }
    return result;
}

void Socket::shutDownWrite()
{
    if (::shutdown(sockfd,SHUT_WR)<0)
    {
        log_error("shut down socket fd %d fail",sockfd);
    }
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}
