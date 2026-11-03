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

// 工作线程管理接口；库实现使用 Thread.h。
class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc func, const std::string& name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { assert(started_); return tid_; }
    const std::string& name() const { return name_; }
    static int numCreated() { return numCreated_.load(); }

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
