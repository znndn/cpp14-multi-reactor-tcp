#pragma once
#include <sys/epoll.h>
#include <vector>
#include "Poller.h"

// epoll LT 实现；每个 EventLoop 持有一个实例。
class EPollPoller : public Poller
{
public:

    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:

    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    void update(int operation, Channel* channel);
    static const int kInitEventListSize = 16;

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    // 就绪事件中的 data.ptr 对应已登记的 Channel。
    EventList events_;
};
