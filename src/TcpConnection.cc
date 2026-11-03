#include <iostream>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include "noncopyable.h"
#include "Logger.h"
#include "InetAddress.h"
#include "Socket.h"
#include "CallBacks.h"
#include "buffer.h"
#include "timestamp.h"
#include "TcpConnection.h"
#include "Channel.h"

// 由 baseLoop 构造，事件注册与后续 I/O 交给选定的 ioLoop。
TcpConnection::TcpConnection(
                EventLoop* loop,
                const std::string& nameArg,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr):

                loop_(loop),name_(nameArg),
                socket_(std::make_unique<Socket>(sockfd)),
                channel_(std::make_unique<Channel>(loop,sockfd)),
                localAddr_(localAddr),
                peerAddr_(peerAddr)
{
    setState(Connecting);
    reading_=false;
    channel_->setReadCallback([this](Timestamp receiveTime){handleRead(receiveTime);});
    channel_->setWriteCallback([this](){handleWrite();});
    channel_->setCloseCallback([this](){handleClose();});
    channel_->setErrorCallback([this](){handleError();});
}
TcpConnection::~TcpConnection()
{
    socket_.reset();
    channel_.reset();
    setState(Disconnected);
}

void TcpConnection::send(const std::string& buf)
{
    // 保留连接与数据副本，避免异步执行时对象或参数已经失效。
    TcpConnectionPtr preserve(shared_from_this());
    std::string message(buf);
    loop_->runInLoop([preserve,message]()
        {
            if (preserve->connected()==true)
            {
                preserve->sendInLoop(message.c_str(),message.length());
            }
        }
    );
}
void TcpConnection::shutdown()
{
    TcpConnectionPtr preserve(shared_from_this());

    // 状态转换与发送任务在同一 loop 排序，避免提前拒绝先前提交的发送。
    loop_->runInLoop([preserve]() {
        if (preserve->state_ != Connected)
            return;
        preserve->setState(Disconnecting);
        preserve->shutdownInLoop();
    });
}

void TcpConnection::shutdownInLoop()
{
    TcpConnectionPtr preserve(shared_from_this());
    if (state_!=Disconnecting) return;
    if (channel_->isWriting()==true) return;
    if (outputBuffer_.readableBytes()!=0)
    {
        return;
    }
    handleWrite();
    socket_->shutDownWrite();
}

void TcpConnection::connectEstablished()
{
    setState(Connected);
    channel_->tie(shared_from_this());
    channel_->enableReading();
    reading_=true;
    log_info("creating tcp connection on fd=%d state=%d\n", channel_->fd(), static_cast<int>(state_));
    if(connectionCallback_!=nullptr)
    {
        connectionCallback_(shared_from_this());
    }
}
// 逻辑断开与最终 Channel 注销分开，避免在事件分发中直接销毁对象。
void TcpConnection::channelDistroy()
{
    if (state_!=Disconnected)
    {
        setState(Disconnected);
        log_info("close tcp connection on fd=%d state=%d\n", channel_->fd(), static_cast<int>(state_));
        channel_->disableAll();
        reading_=false;
        if(connectionCallback_!=nullptr)
        {
            connectionCallback_(shared_from_this());
        }
    }
    if (loop_->hasChannel(channel_.get()))
    {
        loop_->removeChannel(channel_.get());
    }
}

// 接收缓冲区可能仍保留应用层尚未消费的数据。
void TcpConnection::handleRead(Timestamp receiveTime)
{
    if (state_==Disconnected || channel_->isReading()==false) return;
    int Errors;
    int result;

    do
    {
        result = inputBuffer_.readFd(socket_->Getfd(),&Errors);
        Errors=errno;
    } while (result<0 && Errors==EINTR);

    if (result == -1)
    {
        // 当前没有可读数据，等待后续就绪通知。
        if (Errors == EAGAIN || Errors == EWOULDBLOCK)
            return;
        log_error("read failed, fd=%d errno=%d", socket_->Getfd(), Errors);
        handleClose();
        return;
    }
    else if (result==0)
    {
        handleClose();
    }
    else
    {
        if (messageCallback_!=nullptr)
        {
            messageCallback_(shared_from_this(),&inputBuffer_,receiveTime);
        }
        else
        {
            log_debug("fd=%d received %d bytes without messageCallback, discarding",
                 socket_->Getfd(), inputBuffer_.readableBytes());
            inputBuffer_.useAllData();
        }
    }
}
// 只在有写关注时消耗积压数据，排空后关闭写关注。
void TcpConnection::handleWrite()
{
    if (state_==Disconnected || channel_->isWriting()==false) return;
    int Error;
    int result;

    do
    {
        result = outputBuffer_.writeFd(socket_->Getfd(),&Error);
        Error=errno;
    } while (result<0 && Error==EINTR);

    if (result==-1)
    {
        if (Error == EAGAIN || Error == EWOULDBLOCK)
        {
            channel_->enableWriting();
            return;
        }
        else
        {
            handleClose();
        }
    }
    else if (result==0)
    {
        channel_->enableWriting();
        return;
    }
    else
    {
        if (result < outputBuffer_.readableBytes())
        {
            outputBuffer_.useData(result);
            channel_->enableWriting();
            return;
        }
        else if (result == outputBuffer_.readableBytes())
        {
            outputBuffer_.useAllData();
            channel_->disableWriting();
            if (writeCompleteCallback_!=nullptr)
                writeCompleteCallback_(shared_from_this());
            shutdownInLoop();
            return;
        }
    }
}
// 禁用事件并通知应用，再交由 TcpServer 安排表移除及 Channel 注销。
void TcpConnection::handleClose()
{
    if (state_==Disconnected) return;
    setState(Disconnected);
    log_info("close tcp connection on fd=%d state=%d\n", channel_->fd(), static_cast<int>(state_));
    channel_->disableAll();
    reading_=false;
    TcpConnectionPtr guard(shared_from_this());
    if(connectionCallback_!=nullptr)
    {
        connectionCallback_(shared_from_this());
    }
    if (closeCallback_!=nullptr)
        closeCallback_(shared_from_this());
}
void TcpConnection::handleError()
{
    if (state_==Disconnected) return;
    int socketError = 0;
    socklen_t length = sizeof(socketError);
    if (getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &socketError, &length)==-1)
    {
        socketError=errno;
        log_error("Fail to locate errors on socket fd %d with errno %d",socket_->Getfd(),socketError);
        handleClose();
    }
    else if (socketError!=0)
    {
        log_error("socket fd %d failed with errno %d",socket_->Getfd(),socketError);
        handleClose();
    }
}

void TcpConnection::sendInLoop(const char* data, size_t len)
{
    if (state_!=Connected) return;
    else if (len<=0) return;

    // 有积压时追加数据以保持发送顺序，否则尝试直接写 socket。
    if (outputBuffer_.readableBytes()>0 || channel_->isWriting()==true)
    {
        outputBuffer_.bufferAppend(len,data);
    }
    else
    {
        int errors;
        int result;
        do
        {
            result=::write(socket_->Getfd(),data,len);
            errors=errno;
        } while (result<0 && errors==EINTR);

        if (result>0)
        {
            if (result<len)
            {
                int CurrPileUp = outputBuffer_.readableBytes()+(len-result);

                outputBuffer_.bufferAppend(len-result,const_cast<char*>(data)+result);
                channel_->enableWriting();
            }
            else channel_->disableWriting();
        }
        else if (result==0)
        {
            outputBuffer_.bufferAppend(len,const_cast<char*>(data));
            channel_->enableWriting();
            return;
        }
        else
        {
            if (errors==EAGAIN||errors==EWOULDBLOCK)
            {
                outputBuffer_.bufferAppend(len,const_cast<char*>(data));
                channel_->enableWriting();
                return;
            }
            else
            {
                log_error("write failed, fd=%d errno=%d", socket_->Getfd(), errors);
                handleClose();
            }
        }
    }
}
