#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include "noncopyable.h"
#include "InetAddress.h"
#include "CallBacks.h"
#include "buffer.h"
#include "timestamp.h"

class Channel;
class EventLoop;
class Socket;

// 单个 TCP 连接的读写、状态与回调管理；I/O 在所属 loop 串行执行。
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop,
                  const std::string& nameArg,
                  int sockfd,
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection(); // 连接应先于所属 EventLoop 销毁。

    EventLoop* getLoop() const
    {
        return loop_;
    }
    const std::string& name() const
    {
        return name_;
    }
    const InetAddress& localAddress() const
    {
        return localAddr_;
    }
    const InetAddress& peerAddress() const
    {
        return peerAddr_;
    }
    bool connected() const
    {
        return state_ == Connected;
    }

    // 可跨线程调用；任务持有连接及数据副本直至执行。
    void send(const std::string& buf);

    // 输出缓冲排空后半关闭写端。
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb)
    {
        connectionCallback_ = cb;
    }
    void setMessageCallback(const MessageCallback& cb)
    {
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    {
        writeCompleteCallback_ = cb;
    }
    void setCloseCallback(const CloseCallback& cb)
    {
        closeCallback_ = cb;
    }

    void connectEstablished();
    // 在所属 loop 最终注销 Channel，由 TcpServer 安排调用。
    void channelDistroy();

private:
    enum StateE
    {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting
    };
    void setState(StateE state) { state_ = state; }

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const char* data, size_t len);
    void shutdownInLoop();

    EventLoop* loop_;
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_; // 通知应用消费接收缓冲区。

    // handleWrite 排空应用层输出缓冲时通知，不表示对端已收到。
    WriteCompleteCallback writeCompleteCallback_;

    CloseCallback closeCallback_;

    Buffer inputBuffer_; // 接收字节流，消息边界由应用层解释。
    Buffer outputBuffer_; // 保存非阻塞写尚未发送的字节。
};
