#include "Thread.h"
#include "CurrentThread.h"
#include <utility>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string& name)
    : started_(false),
      joined_(false),
      tid_(0),
      func_(std::move(func)),
      name_(name)
{
    assert(func_);
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
    {
        join();
    }
}

void Thread::start()
{
    assert(!started_);

    thread_ = std::thread([this]()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tid_ = CurrentThread::GetTID();
        }
        cond_.notify_one();
        func_();
    });
    started_ = true;

    // 确保 start() 返回时 tid 已初始化。
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this]() { return tid_ != 0; });
}

void Thread::join()
{
    assert(started_);
    assert(CurrentThread::GetTID() != tid_);

    if (joined_)
    {
        return;
    }
    else
    {
        thread_.join();
        joined_ = true;
    }
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        name_ = "Thread" + std::to_string(num);
    }
}
