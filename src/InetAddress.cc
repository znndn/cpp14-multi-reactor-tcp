#include <InetAddress.h>
#include <iostream>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>

InetAddress::InetAddress(uint16_t port,std::string ip)
{
    std::memset(&sockaddr,0,sizeof(sockaddr));
    sockaddr.sin_family=AF_INET;
    sockaddr.sin_port=htons(port);
    sockaddr.sin_addr.s_addr=::inet_addr(ip.c_str());
}

InetAddress::InetAddress(sockaddr_in &sockaddr)
{
    this->sockaddr=sockaddr;
}

std::string InetAddress::toIP() const
{
    char arr[64]={0};
    inet_ntop(AF_INET,&sockaddr.sin_addr,&arr[0],sizeof(arr));
    return arr;
}

std::string InetAddress::toIPPort() const
{
    char arr[64]={0};
    inet_ntop(AF_INET,&sockaddr.sin_addr,&arr[0],sizeof(arr));
    uint16_t port=ntohs(sockaddr.sin_port);
    int ipv4Size=strlen(arr);
    arr[ipv4Size]=':';
    sprintf(&arr[ipv4Size+1],"%u",port);
    return arr;
}

uint16_t InetAddress::toPort() const
{
    return ntohs(sockaddr.sin_port);
}

const sockaddr_in* InetAddress::getSocketAddress() const
{
    return &sockaddr;
}

void InetAddress::setSocketAddress(const sockaddr_in &addr)
{
    sockaddr=addr;
}
