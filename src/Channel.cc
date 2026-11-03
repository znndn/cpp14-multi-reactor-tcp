#include <functional>
#include <memory>
#include <utility>
#include <sys/epoll.h>
#include "noncopyable.h"
#include "timestamp.h"
#include "EventLoop.h"
#include "Channel.h"

class EventLoop;

Channel::Channel(EventLoop* loop, int fd):fd_(fd),loop_(loop)
{
    events_=0;
    revents_=0;
    index_=-1;
    tied=false;
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied==false)
    {
        // 监听与唤醒 Channel 没有 TcpConnection 绑定，仍正常分发。
        handleEventWithGuard(receiveTime);
    }
    else
    {
        // 强引用只覆盖本次事件处理，weak_ptr 避免循环持有连接。
        std::shared_ptr<void> guard=tie_.lock();

        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    // 同时有可读事件时，先由读路径处理剩余数据或 EOF。
    if ((revents_&EPOLLHUP)!=0 && (revents_&EPOLLIN)==0)
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }
    if ((revents_&EPOLLERR)!=0)
    {
        if (errorCallback_)
            errorCallback_();
    }
    if ((revents_&EPOLLIN)!=0 || (revents_&EPOLLPRI)!=0)
    {
        if (readCallback_)
            readCallback_(receiveTime);
    }
    if ((revents_&EPOLLOUT)!=0)
    {
        if (writeCallback_)
            writeCallback_();
    }
}

void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;
    tied = true;
}
int Channel::fd() const
{
    return fd_;
}
int Channel::events() const
{
    return events_;
}
void Channel::set_revents(int revt)
{
    revents_=revt;
}
int Channel::index()
{
    return index_;
}
void Channel::set_index(int idx)
{
    index_=idx;
}

void Channel::enableReading()
{
    events_ = events_ | EPOLLIN;
    events_ = events_ | EPOLLPRI;
    update();
}
void Channel::disableReading()
{
    events_ = events_ & ~EPOLLIN;
    events_ = events_ & ~EPOLLPRI;
    update();
}
void Channel::enableWriting()
{
    events_ = events_ | EPOLLOUT;
    update();
}
void Channel::disableWriting()
{
    events_ = events_ & ~EPOLLOUT;
    update();
}
// 清空事件关注；最终注销仍由 remove() 完成。
void Channel::disableAll()
{
    events_=0;
    update();
}

bool Channel::isNoneEvent() const
{
    return events_==0?1:0;
}
bool Channel::isWriting() const
{
    return (events_&EPOLLOUT)!=0?true:false;
}
bool Channel::isReading() const
{
    return (events_&EPOLLIN)!=0?true:false;
}

EventLoop* Channel::ownerLoop()
{
    return loop_;
}

void Channel::setReadCallback(ReadEventCallback cb)
{
    readCallback_=std::move(cb);
}
void Channel::setWriteCallback(EventCallback cb)
{
    writeCallback_=std::move(cb);
}
void Channel::setCloseCallback(EventCallback cb)
{
    closeCallback_=std::move(cb);
}
void Channel::setErrorCallback(EventCallback cb)
{
    errorCallback_=std::move(cb);
}

void Channel::update()
{
    loop_->updateChannel(this);
}
void Channel::remove()
{
    loop_->removeChannel(this);
}
