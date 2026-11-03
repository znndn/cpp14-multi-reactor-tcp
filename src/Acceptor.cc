#include <functional>
#include <sys/socket.h>

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"
#include "Acceptor.h"
#include "Logger.h"

static int createNonblocking()
{
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                      IPPROTO_TCP);
    if (fd < 0) {
        log_fatal("create listen socket failed, errno=%d", errno);
    }
    return fd;
}
Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport):
    loop_(loop),
    acceptSocket_(createNonblocking()),
    acceptChannel_(loop,acceptSocket_.Getfd()),
    listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.BindAddress(listenAddr);
    acceptChannel_.setReadCallback([this](Timestamp) { handleRead(); });
}

// 先注销监听 Channel，再由 Socket 析构关闭监听 fd。
Acceptor::~Acceptor()
{
    if (loop_-> hasChannel(&acceptChannel_)==true)
    {
        acceptChannel_.disableAll();
        acceptChannel_.remove();
    }
}

void Acceptor::listen()
{
    acceptSocket_.listen();
    listenning_=true;
    acceptChannel_.enableReading();
}

void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd<0)
    {
        // 非阻塞 accept 的暂时失败不终止监听。
        auto saveError=errno;
        if (saveError == EAGAIN || saveError == EWOULDBLOCK||
            saveError == EINTR || saveError == ECONNABORTED)
            return;
        else log_fatal("fail to fetch client InetAddress with errno %d",saveError);
    }
    else
    {
        if (newConnectionCallback_==nullptr)
        {
            ::close(connfd);
            log_fatal("newConnectionCallback_ is nullptr on accept socket %d",connfd);
            return;
        }
        newConnectionCallback_(connfd,peerAddr);
    }
}
