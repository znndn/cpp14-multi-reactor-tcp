#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <sys/epoll.h>
#include "noncopyable.h"
#include "timestamp.h"
#include "EventLoop.h"

class EventLoop;

// 封装 fd 的事件关注与回调分发；不拥有 fd 的关闭职责。
class Channel: noncopyable
{
    public:

    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;
    Channel(EventLoop* loop, int fd);
    ~Channel()=default;

    void handleEvent(Timestamp receiveTime);

    void setReadCallback(ReadEventCallback cb);
    void setWriteCallback(EventCallback cb);
    void setCloseCallback(EventCallback cb);
    void setErrorCallback(EventCallback cb);

    // 事件分发期间临时持有连接，避免回调链释放其所属对象。
    void tie(const std::shared_ptr<void>& obj);
    int fd() const;
    int events() const;
    void set_revents(int revt);
    int index();
    void set_index(int idx);

    void enableReading();
    void disableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();

    bool isNoneEvent() const;
    bool isWriting() const;
    bool isReading() const;

    EventLoop* ownerLoop();

    void remove();

    private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int NoneEvent=0;
    static const int ReadEvent=EPOLLIN|EPOLLPRI;
    static const int WriteEvent=EPOLLOUT;

    const int fd_;
    int events_; // 关注的事件掩码。
    int revents_; // 本轮 epoll 返回的就绪事件。
    int index_; // EPollPoller 中的登记状态：New、Added 或 Deleted。

    std::weak_ptr<void> tie_;
    bool tied;

    EventLoop *loop_;

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
