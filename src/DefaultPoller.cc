#include "Poller.h"
#include "EpollPoller.h"

// 当前默认后端为 epoll；返回对象交由 EventLoop 持有。
Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    return new EPollPoller(loop);
}
