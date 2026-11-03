#include <iostream>
#include <netinet/in.h>
#pragma once

// IPv4 地址与端口封装，内部使用 sockaddr_in。
class InetAddress
{
    private:
    sockaddr_in sockaddr;

    public:
    explicit InetAddress(uint16_t port=0,std::string ip="127.0.0.1");
    explicit InetAddress(sockaddr_in &sockaddr);

    std::string toIP() const;
    std::string toIPPort() const;
    uint16_t toPort() const;

    const sockaddr_in *getSocketAddress() const;
    void setSocketAddress(const sockaddr_in &addr);
};
