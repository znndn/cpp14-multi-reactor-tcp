#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include "EventLoop.h"
#include <cassert>
#include <utility>

EventLoopThreadPool::EventLoopThreadPool
    (EventLoop* baseLoop,const std::string& nameArg)
    :   baseLoop_(baseLoop),
        name_(nameArg),
        started_(false),
        numThreads_(0),
        next_(0)
{
    assert(baseLoop_ != nullptr);
    assert(baseLoop_->isInLoopThread());
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    assert(baseLoop_->isInLoopThread());

    // 清空非拥有索引后，逐个停止并回收工作线程。
    loops_.clear();
    threads_.clear();
}

void EventLoopThreadPool::setThreadNum(int numThreads)
{
    assert(baseLoop_->isInLoopThread());
    assert(!started_ && numThreads >= 0);
    numThreads_ = numThreads;
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
    assert(baseLoop_->isInLoopThread());
    assert(!started_);

    started_ = true;

    for (int i = 0; i < numThreads_; i++)
    {
        auto thread = std::make_unique<EventLoopThread>(cb, name_ + std::to_string(i));

        EventLoop* loop = thread->startLoop();

        threads_.push_back(std::move(thread));
        loops_.push_back(loop);
    }

    // 无额外 I/O 线程时，初始化回调直接作用于 baseLoop。
    if (numThreads_ == 0 && cb!=nullptr)
    {
        cb(baseLoop_);
    }
}

EventLoop* EventLoopThreadPool::getNextLoop()
{
    assert(baseLoop_->isInLoopThread());
    assert(started_);

    if (loops_.size()==0) return baseLoop_;
    else if (loops_.size()==1) return loops_[0];
    else
    {
        int toReturn;
        if (next_==loops_.size()-1)
        {
            toReturn=next_;
            next_=0;
        }
        else
        {
            toReturn=next_;
            next_++;
        }
        return loops_[toReturn];
    }
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    assert(baseLoop_->isInLoopThread());
    assert(started_);

    if (loops_.empty())
    {
        return std::vector<EventLoop*>{baseLoop_};
    }
    return loops_;
}
