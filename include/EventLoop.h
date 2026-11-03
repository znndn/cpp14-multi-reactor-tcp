#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include "noncopyable.h"
#include "timestamp.h"

class Channel;
class Poller;

// 每线程一个事件循环，串行分发 I/O 回调并执行已投递的任务。
class EventLoop : noncopyable
{
public:

    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void quit();
    Timestamp pollReturnTime() const;

    // 所属线程直接执行，其他线程转入任务队列。
    void runInLoop(Functor cb);
    // 始终入队，在事件处理之后执行；可从其他线程调用。
    void queueInLoop(Functor cb);
    void wakeup();

    // Channel 登记和注销应在所属 loop 线程执行。
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);
    bool isInLoopThread() const;

private:

    void handleRead();
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;
    std::atomic_bool quit_; // 退出请求在当前轮处理结束后生效。

    const pid_t threadId_;
    Timestamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;

    // eventfd 仅用于唤醒，任务内容保存在 pendingFunctors_。
    int wakeupFd_;

    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_; // 仅保存本轮就绪的 Channel。
    std::atomic_bool callingPendingFunctors_;
    std::vector<Functor> pendingFunctors_;

    std::mutex mutex_; // 保护跨线程投递的任务队列。
};
