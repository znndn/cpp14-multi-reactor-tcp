# C++14 Multi-Reactor TCP Network Library

基于 C++14 的 Linux TCP 网络库，参考陈硕老师的 [Muduo](https://github.com/chenshuo/muduo) 的 Reactor 架构实现。使用非阻塞 Socket、epoll LT 与 I/O 线程池，提供连接管理、缓冲读写、跨线程任务投递及应用回调接口。

## 具体实现

- **主从 Reactor**：主线程的 `Acceptor` 接收连接，`EventLoopThreadPool` 轮询选择 I/O 线程；每个线程运行一个 `EventLoop`，串行处理所属连接的读写事件。
- **任务投递**：`runInLoop` 在所属线程直接执行，跨线程调用转入任务队列；`queueInLoop` 始终入队，通过互斥锁保护队列、通过 `eventfd` 唤醒事件循环。
- **缓冲读写**：连接分别持有输入与输出 `Buffer`。接收端通过 `readv` 使用现有可写区与 64 KiB 栈上备用区；发送端先尝试直接写入，剩余数据交由输出缓冲区和可写事件继续发送。
- **生命周期管理**：`Socket` 负责关闭 fd，`unique_ptr` 管理内部资源；连接由 `shared_ptr` 持有，`Channel::tie` 使用 `weak_ptr` 在事件分发期间取得临时强引用。
- **连接回收与停机**：连接表在主线程维护，Channel 在所属 I/O 线程注销；主线程收到清理完成通知后，再回收 I/O 线程池。

## 模块关系

```text
baseLoop / 主线程
  Acceptor ── 接收连接 ──> TcpServer / 连接表
                              │ 轮询分配
                    EventLoopThreadPool
                       ├─ I/O 线程 1：EventLoop → EPollPoller → Channel
                       ├─ I/O 线程 2：EventLoop → EPollPoller → Channel
                       └─ ...                                  │
                                                       TcpConnection
                                                        Buffer / Socket
```

## 构建与运行

依赖 Linux、支持 C++14 的 GCC/Clang、CMake 3.16.3 或更高版本，以及 POSIX 线程库。

```bash
git clone https://github.com/znndn/cpp14-multi-reactor-tcp.git
cd cpp14-multi-reactor-tcp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
./build/testserver
```


