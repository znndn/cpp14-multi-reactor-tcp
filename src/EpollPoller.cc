#include <sys/epoll.h>
#include <vector>
#include <unistd.h>
#include <errno.h>
#include <string>

#include "Poller.h"
#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"
#include "timestamp.h"

// Channel 在 Poller 中的登记状态，与事件关注掩码分别维护。
constexpr int New=-1;
constexpr int Added=1;
constexpr int Deleted=2;

EPollPoller::EPollPoller(EventLoop* loop):Poller(loop)
{
    events_.resize(kInitEventListSize);
    auto result=epoll_create1(EPOLL_CLOEXEC);
    if (result==-1)
    {
        log_fatal("unable to create epoll fd");
    }
    else epollfd_=result;
}
EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

void EPollPoller::update(int operation, Channel* channel)
{
    epoll_event TempEvent;
    TempEvent.events=channel->events();
    TempEvent.data.ptr=channel;
    int result=epoll_ctl(epollfd_, operation, channel->fd(), &TempEvent);

    if (result==-1)
    {
        if (operation==EPOLL_CTL_DEL)
            log_error("fail to update epoll fd %d with errno %d",epollfd_,errno);
        else
            log_fatal("fail to update epoll fd %d with errno %d",epollfd_,errno);
    }
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    int result=epoll_wait(epollfd_, &events_[0], events_.size(), timeoutMs);
    auto CheckErrno=errno;
    auto curr=Timestamp::now();

    if (result==-1)
    {
        if (CheckErrno==EINTR) return curr;
        std::string now=curr.toString();
        log_error("epoll wait failed with errno %d at %s",CheckErrno, now.c_str());
    }
    else if (result>0)
    {
        fillActiveChannels(result,activeChannels);

        // 本轮填满事件数组时扩容，供后续 epoll_wait 使用。
        if (static_cast<long unsigned int>(result) == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    return curr;
}

void EPollPoller::updateChannel(Channel* channel)
{
    if (channel->index()==New)
    {
        channels_[channel->fd()]=channel;
        channel->set_index(Added);
        update(EPOLL_CTL_ADD,channel);
    }
    else if (channel->index()==Added)
    {
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(Deleted);
        }
        else
        {
            update(EPOLL_CTL_MOD,channel);
        }
    }
    else
    {
        channel->set_index(Added);
        update(EPOLL_CTL_ADD,channel);
    }
}
// 注销事件登记及映射，不关闭 fd；fd 由 Socket 或 EventLoop 持有。
void EPollPoller::removeChannel(Channel* channel)
{
    channels_.erase(channel->fd());
    if (channel->index()==Added)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(New);
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    for (int i=0;i<numEvents;i++)
    {
        static_cast<Channel*>(events_[i].data.ptr)->set_revents(events_[i].events);
        activeChannels->push_back(static_cast<Channel*>(events_[i].data.ptr));
    }
}
