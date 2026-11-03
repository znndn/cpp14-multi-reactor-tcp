#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <thread>
#include <cerrno>
#include <cstdint>
#include <sys/eventfd.h>
#include "noncopyable.h"
#include "timestamp.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"
#include "CurrentThread.h"

// 检查同一线程中是否已存在 EventLoop。
thread_local EventLoop* t_loopInThisThread = nullptr;

class Channel;
class Poller;

int createEventfd()
{
    int result=::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    auto SaveError=errno;
    if (result==-1)
    {
        log_fatal("fail to create eventfd with errno %d",SaveError);
    }
    else
    {
        return result;
    }
}

EventLoop::EventLoop():
    threadId_(CurrentThread::GetTID()),
    poller_(Poller::newDefaultPoller(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this,wakeupFd_))
{
    looping_=false;
    quit_=false;
    callingPendingFunctors_=false;

    if (t_loopInThisThread!=nullptr) log_fatal("found duplicate construction of EventLoop");
    else t_loopInThisThread=this;

    wakeupChannel_->setReadCallback([this](Timestamp) { handleRead(); });
    wakeupChannel_->enableReading();
}

// 销毁 loop 前，其业务连接应已完成 Channel 注销。
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    wakeupChannel_.reset();
    ::close(wakeupFd_);
    t_loopInThisThread=nullptr;
}

void EventLoop::loop()
{
    looping_=true;
    quit_=false;
    while (quit_==false)
    {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(10000,&activeChannels_);
        // 先分发本轮 I/O，再执行任务；Channel 的最终注销可延后到任务阶段。
        for (auto elem: activeChannels_)
        {
            elem->handleEvent(pollReturnTime_);
        }
        doPendingFunctors();
    }
    looping_=false;
}

void EventLoop::quit()
{
    quit_=true;
    if (looping_) wakeup();
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()==true) cb();
    else queueInLoop(cb);
}

void EventLoop::queueInLoop(Functor cb)
{
    if (true)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }

    if (isInLoopThread()==false)
    {
        wakeup();
        return;
    }
    // 当前任务批次中再次入队时也唤醒，避免下一轮先阻塞在 epoll_wait。
    else if (isInLoopThread()==true && callingPendingFunctors_==true)
    {
        wakeup();
        return;
    }
    // loop 启动前在本线程入队不会主动唤醒，可能等待首次 poll 超时。
    else return;
}

bool EventLoop::isInLoopThread() const
{
    if (CurrentThread::GetTID()==threadId_)
    return true;
    else return false;
}

Timestamp EventLoop::pollReturnTime() const
{
    return pollReturnTime_;
}

void EventLoop::wakeup()
{
    uint64_t one=1;
    ssize_t n=::write(wakeupFd_,&one,sizeof(one));
    auto saveErrno=errno;
    if (n!=8) log_error("cannot wake up thread %d via wakeup fd %d correctly with errno %d"
        ,threadId_,wakeupFd_,saveErrno);
}

void EventLoop::updateChannel(Channel* channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel* channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel* channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::handleRead()
{
    uint64_t count = 0;
    // 普通 eventfd 的一次读取取走累计通知并将计数清零。
    ssize_t n = ::read(wakeupFd_, &count, sizeof(count));
    auto saveErrno=errno;
    if (n!=8) log_error("cannot reset wakeup fd %d correctly with errno %d"
        ,wakeupFd_,saveErrno);
}

void EventLoop::doPendingFunctors()
{
    callingPendingFunctors_=true;
    std::vector<Functor> functors;
    if (true)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.assign(pendingFunctors_.begin(), pendingFunctors_.end());
        pendingFunctors_.clear();
    }
    // 锁外执行回调，允许回调继续提交任务。
    for (Functor i : functors)
    {
        i();
    }
    callingPendingFunctors_=false;
}
