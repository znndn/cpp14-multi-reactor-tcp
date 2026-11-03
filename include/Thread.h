#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <sys/types.h>
#include "noncopyable.h"

// 工作线程的启动与 join 管理；控制方法由创建者线程调用。
class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc func, const std::string& name = std::string());
    ~Thread();

    // 等待工作线程发布 tid 后返回。
    void start();
    void join();

    bool started() const
    {
        return started_;
    }
    pid_t tid() const
    {
        assert(started_); return tid_;
    }
    const std::string& name() const
    {
        return name_;
    }
    static int numCreated()
    {
        return numCreated_.load();
    }

private:
    void setDefaultName();

    bool started_;
    bool joined_;

    std::thread thread_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;

    std::mutex mutex_;
    std::condition_variable cond_;
    static std::atomic_int numCreated_;
};
