#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "noncopyable.h"

class EventLoop;
class EventLoopThread;

// 由 baseLoop 管理 I/O 线程，并以轮询方式分配新连接。
class EventLoopThreadPool : noncopyable
{
public:

    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
    ~EventLoopThreadPool();

    // 指定额外 I/O 线程数；为 0 时由 baseLoop 处理连接。
    void setThreadNum(int numThreads);

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    EventLoop* getNextLoop();
    std::vector<EventLoop*> getAllLoops();

    bool started() const
    {
        return started_;
    }
    const std::string& name() const
    {
        return name_;
    }

private:

    EventLoop* baseLoop_;
    std::string name_;

    bool started_;
    int numThreads_;

    size_t next_;

    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    // 非拥有指针，与 threads_ 一一对应。
    std::vector<EventLoop*> loops_;
};
