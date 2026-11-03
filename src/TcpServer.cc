#include "TcpServer.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "Logger.h"
#include <cassert>
#include <cerrno>
#include <utility>
#include <sys/socket.h>
#include <unistd.h>

static EventLoop* checkedLoop(EventLoop* loop)
{
    if (loop == nullptr)
        log_fatal("TcpServer needs a baseLoop");
    assert(loop->isInLoopThread());
    return loop;
}

static bool readLocalAddress(int sockfd, InetAddress* localAddr)
{
    sockaddr_in local{};
    socklen_t length = sizeof(local);

    if (getsockname(sockfd, reinterpret_cast<sockaddr*>(&local), &length) < 0)
    {
        int error = errno;
        log_error("getsockname failed, fd=%d errno=%d", sockfd, error);
        return false;
    }

    localAddr->setSocketAddress(local);
    return true;
}

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr,
                     const std::string& nameArg, Option option)
    : loop_(checkedLoop(loop)),
      ipPort_(listenAddr.toIPPort()),
      name_(nameArg),
      acceptor_(std::make_unique<Acceptor>(loop_, listenAddr,
                                         option == ReusePort)),
      threadPool_(std::make_unique<EventLoopThreadPool>(loop_, name_)),
      started_(false),
      nextConnId_(1),
      stopping_(false),
      stopped_(false),
      pendingCleanups_(0)
{
    acceptor_->setNewConnectionCallback(
        [this](int fd, const InetAddress& peerAddr)
        {
            newConnection(fd, peerAddr);
        });
}

TcpServer::~TcpServer()
{
    assert(loop_->isInLoopThread());

    // 应用必须等待 stop 完成回调，再析构服务器及 baseLoop。
    if (started_ && !stopped_)
        log_fatal("TcpServer must finish stop before destruction");

    assert(connections_.empty() && pendingCleanups_ == 0);
}

void TcpServer::setThreadNum(int numThreads)
{
    assert(loop_->isInLoopThread() && !started_);
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::setThreadInitCallback(const ThreadInitCallback& cb)
{
    assert(loop_->isInLoopThread() && !started_);
    threadInitCallback_ = cb;
}

void TcpServer::setConnectionCallback(const ConnectionCallback& cb)
{
    assert(loop_->isInLoopThread() && !started_);
    connectionCallback_ = cb;
}

void TcpServer::setMessageCallback(const MessageCallback& cb)
{
    assert(loop_->isInLoopThread() && !started_);
    messageCallback_ = cb;
}

void TcpServer::start()
{
    assert(loop_->isInLoopThread());

    if (started_)
        return;
    started_ = true;

    threadPool_->start(threadInitCallback_);
    acceptor_->listen();
}

void TcpServer::stop(StopCallback cb)
{
    assert(loop_->isInLoopThread());
    assert(started_);

    if (stopping_)
        return;

    stopping_ = true;
    stopCallback_ = std::move(cb);

    // 延后至任务阶段销毁 Acceptor，避开当前轮的事件分发。
    loop_->queueInLoop([this]()
    {
        acceptor_.reset();
        maybeFinishStop();
    });

    loop_->wakeup();
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    assert(loop_->isInLoopThread());

    if (stopping_)
    {
        close(sockfd);
        return;
    }

    InetAddress localAddr;
    if (readLocalAddress(sockfd, &localAddr)==false)
    {
        close(sockfd);
        return;
    }

    std::string connName = name_ + "-" + ipPort_ + "#" + std::to_string(nextConnId_);

    nextConnId_++;

    // 轮询选择 I/O loop，后续连接读写固定在该 loop。
    auto ioLoop = threadPool_->getNextLoop();

    TcpConnectionPtr conn=std::make_unique<TcpConnection>(ioLoop,connName,sockfd,localAddr,peerAddr);

    connections_[connName]=conn;

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setCloseCallback([this](const TcpConnectionPtr& closing)
    {
        removeConnection(closing);
    });

    // 在所属 loop 中完成 Channel 绑定及读事件注册。
    ioLoop->runInLoop([conn]()
    {
        conn->connectEstablished();
    });
}

// 连接表只在 baseLoop 修改，关闭通知可能来自 I/O 线程。
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop([this,conn]()
    {
        removeConnectionInLoop(conn);
    });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    assert(loop_->isInLoopThread());

    auto item = connections_.find(conn->name());
    if (item == connections_.end())
        return;

    assert(item->second == conn);

    pendingCleanups_++;
    auto ioLoop = conn->getLoop();
    connections_.erase(item);

    // 延后注销，任务持有连接直到 Channel 已从 Poller 移除。
    ioLoop->queueInLoop([this,conn]()
    {
        conn->channelDistroy();
        reportCleanupDone();
    });
}

void TcpServer::reportCleanupDone()
{
    loop_->queueInLoop([this]()
    {
        assert(pendingCleanups_ > 0);
        pendingCleanups_--;
        maybeFinishStop();
    });
}

void TcpServer::maybeFinishStop()
{
    assert(loop_->isInLoopThread());

    if (!stopping_ || stopped_ || acceptor_!=nullptr || !connections_.empty()|| pendingCleanups_ != 0)
        return;

    // 连接表与待清理计数均清空后，才停止并回收 I/O 线程。
    threadPool_.reset();
    stopped_ = true;
    StopCallback cb = std::move(stopCallback_);
    if (cb)
        cb();
}
