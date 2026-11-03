#include "EventLoopThread.h"
#include "EventLoop.h"
#include "CurrentThread.h"
#include <cassert>

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,const std::string& name)
    : loop_(nullptr),
      exiting_(false),
      ownerThreadId_(CurrentThread::GetTID()),
      thread_([this]() { threadFunc(); }, name),
      callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    stop();
}

EventLoop* EventLoopThread::startLoop()
{
    assert(CurrentThread::GetTID() == ownerThreadId_);
    assert(!exiting_ && !thread_.started());
    thread_.start();

    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock,[this] {return loop_!=nullptr;});
    return loop_;
}

void EventLoopThread::stop()
{
    assert(CurrentThread::GetTID() == ownerThreadId_);
    if (exiting_==true || thread_.started()==false)
    {
        exiting_=true;
        return;
    }
    else exiting_=true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (loop_ != nullptr)
        {
            EventLoop* loop = loop_;
            loop->queueInLoop([loop]() { loop->quit(); });
        }
    }
    // join 时不持有 mutex_，工作线程退出前还需要此锁清空 loop_。
    thread_.join();
}

void EventLoopThread::threadFunc()
{
    EventLoop loop;
    if (callback_!=nullptr)
    {
        callback_(&loop);
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_=&loop;
    }
    cond_.notify_one();
    // 发布完成后释放 mutex_，再进入长期运行的事件循环。
    loop.loop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = nullptr;
    }
}
