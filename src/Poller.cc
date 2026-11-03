#include <vector>
#include <unordered_map>
#include "timestamp.h"
#include "Poller.h"

Poller::Poller(EventLoop* loop): ownerLoop_(loop)
{
}

bool Poller::hasChannel(Channel* channel) const
{
    for (auto it = channels_.begin();it!=channels_.end();it++)
    {
        if (it->second==channel)
        {
            return true;
        }
    }
    return false;
}
