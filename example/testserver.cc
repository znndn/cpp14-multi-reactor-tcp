#include "TcpServer.h"
#include "EventLoop.h"
#include "TcpConnection.h"
#include "InetAddress.h"
#include "Logger.h"
#include "buffer.h"
#include "timestamp.h"

#include <csignal>
#include <string>

// 字节流回声示例：TcpServer 处理连接，应用回调消费并回送数据。
class EchoServer
{
public:
    EchoServer(EventLoop* loop, const InetAddress& addr, const std::string& name)
        : server_(loop, addr, name)
    {
        server_.setConnectionCallback([this](const TcpConnectionPtr& conn)
        {
            onConnection(conn);
        });

        server_.setMessageCallback(
            [this](const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
            {
                onMessage(conn, buf, time);
            });
        server_.setThreadNum(10); // 额外启动 10 个 I/O 线程。
    }

    void start()
    {
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        log_info("Connection %s : %s",
                 conn->connected() ? "UP" : "DOWN",
                 conn->peerAddress().toIPPort().c_str());
    }

    // 按可读字节回显，不假定一次回调对应一条完整消息。
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
    }

    TcpServer server_;
};

int main()
{
    // 对端断开后的写错误交给库处理，避免 SIGPIPE 直接终止进程。
    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        return 1;

    EventLoop loop;
    InetAddress addr(8080, "127.0.0.1");
    EchoServer server(&loop, addr, "EchoServer");
    server.start();
    log_info("EchoServer listening on %s", addr.toIPPort().c_str());
    loop.loop();
}
