#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "noncopyable.h"
#include "CallBacks.h"

class Acceptor;
class EventLoop;
class EventLoopThreadPool;
class InetAddress;

// 在 baseLoop 组装 Acceptor、I/O 线程池和连接表。
class TcpServer : noncopyable
{
public:

    using ThreadInitCallback = std::function<void(EventLoop*)>;
    using StopCallback = std::function<void()>;

    enum Option { NoReusePort, ReusePort };

    TcpServer(EventLoop* loop, const InetAddress& listenAddr,
              const std::string& nameArg, Option option = NoReusePort);
    ~TcpServer();

    void setThreadNum(int numThreads);
    void setThreadInitCallback(const ThreadInitCallback& cb);
    void setConnectionCallback(const ConnectionCallback& cb);
    void setMessageCallback(const MessageCallback& cb);

    void start();
    // 停止接入；等待现有连接结束和清理完成后通知，不强制断开客户端。
    void stop(StopCallback cb = StopCallback());

private:
    void newConnection(int sockfd, const InetAddress& peerAddr);

    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    // I/O 线程报告 Channel 注销完成，由 baseLoop 更新待清理计数。
    void reportCleanupDone();
    void maybeFinishStop();

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop* loop_;
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;
    std::unique_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    ThreadInitCallback threadInitCallback_;

    bool started_;

    std::uint64_t nextConnId_;

    ConnectionMap connections_; // baseLoop 持有活动连接的强引用。

    // 停机状态和清理计数仅由 baseLoop 修改。
    bool stopping_;
    bool stopped_;

    std::size_t pendingCleanups_;
    StopCallback stopCallback_;
};
