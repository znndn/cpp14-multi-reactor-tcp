#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include "noncopyable.h"
#include "Thread.h"

class EventLoop;

// 在工作线程栈上创建并运行 EventLoop，由创建者负责启动和回收。
class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    explicit EventLoopThread(
        const ThreadInitCallback& cb = ThreadInitCallback(),
        const std::string& name = std::string()
);

    ~EventLoopThread();

    // 等待 loop 地址发布；返回的指针不拥有 loop。
    EventLoop* startLoop();
    // 向 loop 投递退出请求，然后等待工作线程结束。
    void stop();

private:
    void threadFunc();

    EventLoop* loop_;

    bool exiting_;
    const pid_t ownerThreadId_;
    Thread thread_;

    std::mutex mutex_; // 保护 loop_ 的发布与清空。
    std::condition_variable cond_;

    ThreadInitCallback callback_;
};
