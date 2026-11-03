#pragma once
#include <vector>
#include <unordered_map>
#include "timestamp.h"

class Channel;
class EventLoop;

// EventLoop 的 I/O 多路复用接口。
class Poller
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller() = default;

    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;
    bool hasChannel(Channel* channel) const;
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    // fd 到 Channel 的非拥有映射，包含连接、监听和唤醒 fd。
    ChannelMap channels_;

private:
    EventLoop* ownerLoop_;
};
