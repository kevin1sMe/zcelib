#include "zce_predefine.h"
#include "zce_os_adapt_predefine.h"
#include "zce_trace_log_debug.h"
#include "zce_os_adapt_error.h"
#include "zce_os_adapt_socket.h"

//
ssize_t ZCE_OS::writev (ZCE_SOCKET handle,
                        const iovec *buffers,
                        int iovcnt)
{
#if defined (ZCE_OS_WINDOWS)

    assert(iovcnt <= IOV_MAX);

    WSABUF wsa_buf[IOV_MAX];

    for (int i = 0; i < iovcnt; ++i)
    {
        wsa_buf[i].buf = static_cast<CHAR *>(buffers[i].iov_base);
        wsa_buf[i].len = static_cast<ULONG>(buffers[i].iov_len);
    }

    DWORD bytes_sent = 0;
    int zce_result = ::WSASend ((SOCKET) handle,
                                wsa_buf,
                                iovcnt,
                                &bytes_sent,
                                0,
                                0,
                                0);

    if (zce_result == SOCKET_ERROR)
    {
        errno = ::WSAGetLastError ();
        return -1;
    }

    return (ssize_t) bytes_sent;

#elif defined (ZCE_OS_LINUX)

    return ::writev (handle, buffers, iovcnt);
#endif
}

//
ssize_t ZCE_OS::readv (ZCE_SOCKET handle,
                       iovec *buffers,
                       int iovcnt)
{
#if defined (ZCE_OS_WINDOWS)

    DWORD bytes_received = 0;
    int result = 1;

    // Winsock 2 has WSARecv and can do this directly, but Winsock 1 needs
    // to do the recvs piece-by-piece.

    //IOV_MAX根据各个平台不太一样
    assert(iovcnt <= IOV_MAX);

    WSABUF wsa_buf[IOV_MAX];

    for (int i = 0; i < iovcnt; ++i)
    {
        wsa_buf[i].buf = static_cast<CHAR *>(buffers[i].iov_base);
        wsa_buf[i].len = static_cast<ULONG>(buffers[i].iov_len);
    }

    DWORD flags = 0;
    result = ::WSARecv ( handle,
                         wsa_buf,
                         iovcnt,
                         &bytes_received,
                         &flags,
                         0,
                         0);

    if (result == SOCKET_ERROR)
    {
        errno = ::WSAGetLastError ();
        return -1;
    }

    return (ssize_t) bytes_received;

#elif defined (ZCE_OS_LINUX)

    return ::readv (handle, buffers, iovcnt);

#endif
}

//
ssize_t ZCE_OS::recvmsg (ZCE_SOCKET handle,
                         msghdr *msg,
                         int flags)
{

#if defined (ZCE_OS_WINDOWS)

    assert(msg->msg_iovlen < IOV_MAX);

    WSABUF wsa_buf[IOV_MAX];

    for (size_t i = 0; i < msg->msg_iovlen; ++i)
    {
        wsa_buf[i].buf = static_cast<CHAR *>(msg->msg_iov[i].iov_base);
        wsa_buf[i].len = static_cast<ULONG>(msg->msg_iov[i].iov_len);
    }

    DWORD bytes_received = 0;

    int result = ::WSARecvFrom ((SOCKET) handle,
                                wsa_buf,
                                (DWORD)(msg->msg_iovlen),
                                &bytes_received,
                                (DWORD *) &flags,
                                static_cast<sockaddr *>(msg->msg_name),
                                &msg->msg_namelen,
                                0,
                                0);

    if (result != 0)
    {
        errno = ::WSAGetLastError ();
        return -1;
    }

    return bytes_received;

#elif defined (ZCE_OS_LINUX)

    return ::recvmsg (handle, msg, flags);

#endif
}

ssize_t ZCE_OS::sendmsg (ZCE_SOCKET handle,
                         const struct msghdr *msg,
                         int flags)
{
#if defined (ZCE_OS_WINDOWS)

    //
    assert(msg->msg_iovlen < IOV_MAX);

    WSABUF wsa_buf[IOV_MAX];

    for (size_t i = 0; i < msg->msg_iovlen; ++i)
    {
        wsa_buf[i].buf = static_cast<CHAR *>(msg->msg_iov[i].iov_base);
        wsa_buf[i].len = static_cast<ULONG>(msg->msg_iov[i].iov_len);
    }

    DWORD bytes_sent = 0;
    int result = ::WSASendTo ((SOCKET) handle,
                              wsa_buf,
                              (DWORD)msg->msg_iovlen,
                              &bytes_sent,
                              flags,
                              static_cast<const sockaddr *>(msg->msg_name),
                              msg->msg_namelen,
                              0,
                              0);

    if (result != 0)
    {
        errno = ::WSAGetLastError ();
        return -1;
    }

    return (ssize_t) bytes_sent;

#elif defined (ZCE_OS_LINUX)
    //
    return ::sendmsg (handle, msg, flags);
# endif

}

int ZCE_OS::socket_init (int version_high, int version_low)
{
#if defined (ZCE_OS_WINDOWS)

    WORD version_requested = MAKEWORD (version_high, version_low);
    WSADATA wsa_data;
    ::WSASetLastError(0);
    int error = ::WSAStartup (version_requested, &wsa_data);

    if (error != 0)
    {
        errno = ::WSAGetLastError ();
        ::fprintf (stderr,
                   "ZCE_OS::socket_init; WSAStartup failed, "
                   "WSAGetLastError returned %d\n",
                   errno);
    }

# else
    ZCE_UNUSED_ARG (version_high);
    ZCE_UNUSED_ARG (version_low);
# endif
    return 0;
}

int ZCE_OS::socket_fini (void)
{
#if defined (ZCE_OS_WINDOWS)

    if (WSACleanup () != 0)
    {
        errno = ::WSAGetLastError ();

        ::fprintf (stderr,
                   "ZCE_OS::socket_fini; WSACleanup failed, "
                   "WSAGetLastError returned %d\n",
                   errno);
    }

# endif
    return 0;
}

//--------------------------------------------------------------------------------------------
//尽量收取len个数据，直到出现错误
ssize_t ZCE_OS::recvn (ZCE_SOCKET handle,
                       void *buf,
                       size_t len,
                       int flags )
{
    ssize_t result = 0;
    bool error_occur = false;

    ssize_t onetime_recv = 0, bytes_recv = 0;

    //一定准备确保收到这么多字节
    for (bytes_recv = 0; static_cast<size_t>(bytes_recv) < len; bytes_recv += onetime_recv)
    {

        //使用端口进行接收
        onetime_recv = ZCE_OS::recv (handle,
                                     static_cast <char *> (buf) + bytes_recv,
                                     len - bytes_recv,
                                     flags);

        if (onetime_recv > 0)
        {
            continue;
        }
        //如果出现错误,退出循环
        else
        {
            //出现错误，进行处理
            error_occur = true;
            result = onetime_recv;
            break;
        }
    }

    //如果发生错误
    if (error_occur)
    {
        return result;
    }

    return bytes_recv;
}

//尽量发送N个数据，直到出现错误
ssize_t ZCE_OS::sendn (ZCE_SOCKET handle,
                       const void *buf,
                       size_t len,
                       int flags)
{
    bool error_occur = false;
    ssize_t result = 0, bytes_send = 0, onetime_send = 0;

    //一定准备确保收到这么多字节，但是一旦出现错误，就退出
    for (bytes_send = 0; static_cast<size_t>(bytes_send) < len; bytes_send += onetime_send)
    {
        //发送数据，，
        onetime_send = ZCE_OS::send (handle,
                                     static_cast <const char *> (buf) + bytes_send,
                                     len - bytes_send,
                                     flags);

        if (onetime_send > 0)
        {
            continue;
        }
        //如果出现错误,== 0一般是是端口断开，==-1表示错误
        else
        {
            //出现错误，进行处理
            error_occur = true;
            result = onetime_send;
            break;
        }
    }

    //发送了错误，返回错误返回值
    if (error_occur)
    {
        return result;
    }

    return bytes_send;
}

//打开某些选项，WIN32目前只支持O_NONBLOCK
int ZCE_OS::sock_enable (ZCE_SOCKET handle, int flags)
{

#if defined (ZCE_OS_WINDOWS)

    switch (flags)
    {
        case O_NONBLOCK:
            // nonblocking argument (1)
            // blocking:            (0)
        {
            u_long nonblock = 1;
            int zce_result = ::ioctlsocket (handle, FIONBIO, &nonblock);

            //将错误信息设置到errno，详细请参考上面ZCE_OS名字空间后面的解释
            if ( SOCKET_ERROR == zce_result)
            {
                errno = ::WSAGetLastError ();
            }

            return zce_result;
        }

        default:
        {
            return (-1);
        }

    }

#elif defined (ZCE_OS_LINUX)
    int val = ::fcntl (handle, F_GETFL, 0);

    if (val == -1)
    {
        return -1;
    }

    // Turn on flags.
    ZCE_SET_BITS (val, flags);

    if (::fcntl (handle, F_SETFL, val) == -1)
    {
        return -1;
    }

    return 0;
#endif
}

//关闭某些选项，WIN32目前只支持O_NONBLOCK
int ZCE_OS::sock_disable(ZCE_SOCKET handle, int flags)
{
#if defined (ZCE_OS_WINDOWS)

    switch (flags)
    {
        case O_NONBLOCK:
            // nonblocking argument (1)
            // blocking:            (0)
        {
            u_long nonblock = 0;
            int zce_result =  ::ioctlsocket (handle, FIONBIO, &nonblock);

            //将错误信息设置到errno，详细请参考上面ZCE_OS名字空间后面的解释
            if ( SOCKET_ERROR == zce_result)
            {
                errno = ::WSAGetLastError ();
            }

            return zce_result;
        }

        default:
            return (-1);
    }

#elif defined (ZCE_OS_LINUX)
    int val = ::fcntl (handle, F_GETFL, 0);

    if (val == -1)
    {
        return -1;
    }

    // Turn on flags.
    ZCE_CLR_BITS (val, flags);

    if (::fcntl (handle, F_SETFL, val) == -1)
    {
        return -1;
    }

    return 0;
#endif
}

//检查在（一定时间内），某个SOCKET句柄关注的单个事件是否触发，如果触发，返回触发事件个数，如果成功，一般触发返回值都是1
int ZCE_OS::handle_ready (ZCE_SOCKET handle,
                          ZCE_Time_Value *timeout_tv,
                          HANDLE_READY_TODO ready_todo)
{

    fd_set handle_set;
    FD_ZERO(&handle_set);

    //whlie(0)会产生一个告警，这个应该是windows内部自己没有处理好。微软说VS2005SP1就修复了，见鬼。
#if defined (ZCE_OS_WINDOWS)
#pragma warning(disable : 4127)
#endif
    FD_SET(handle, &handle_set);
#if defined (ZCE_OS_WINDOWS)
#pragma warning(default : 4127)
#endif

    // Wait for data or for the timeout_tv to elapse.
    int select_width = 0;

#if defined (ZCE_OS_WINDOWS)
    // This arg is ignored on Windows and causes pointer truncation
    // warnings on 64-bit compiles.
    select_width = 0;
#elif defined (ZCE_OS_LINUX)
    select_width = int (handle) + 1;
#endif

    int result = ZCE_OS::select (select_width,
                                 (ready_todo == HANDLE_READY_READ ) ? &handle_set : 0,
                                 (ready_todo == HANDLE_READY_WRITE ) ? &handle_set : 0,
                                 (ready_todo == HANDLE_READY_EXCEPTION ) ? &handle_set : 0,
                                 timeout_tv);

    if (0 == result )
    {
        errno = ETIMEDOUT;
        return 0;
    }

    return result;
}

//检查在（一定时间内），某个SOCKET句柄关注的多个事件是否触发
int ZCE_OS::handle_multi_ready (ZCE_SOCKET handle,
                                ZCE_Time_Value *timeout_tv,
                                int multi_ready_todo,
                                int *multiready_occur)
{
    fd_set handle_set_read, handle_set_write, handle_set_exeception;
    fd_set *p_set_read  = NULL, *p_set_write = NULL, *p_set_exception = NULL;
    FD_ZERO(&handle_set_read);
    FD_ZERO(&handle_set_write);
    FD_ZERO(&handle_set_exeception);

    *multiready_occur = 0;

    //FD_SET 里面的 whlie(0)会产生一个告警，这个应该是windows内部自己没有处理好。微软说VS2005SP1就修复了，见鬼。
#if defined (ZCE_OS_WINDOWS)
#pragma warning(disable : 4127)
#endif

    if ( ZCE_BIT_IS_SET(multi_ready_todo, HANDLE_READY_READ) )
    {
        FD_SET(handle, &handle_set_read);
        p_set_read = &handle_set_read;
    }

    if ( ZCE_BIT_IS_SET(multi_ready_todo, HANDLE_READY_WRITE) )
    {
        FD_SET(handle, &handle_set_write);
        p_set_write = &handle_set_write;
    }

    if ( ZCE_BIT_IS_SET(multi_ready_todo, HANDLE_READY_EXCEPTION) )
    {
        FD_SET(handle, &handle_set_exeception);
        p_set_exception = &handle_set_exeception;
    }

#if defined (ZCE_OS_WINDOWS)
#pragma warning(default : 4127)
#endif

    // Wait for data or for the timeout_tv to elapse.
    int select_width = 0;

#if defined (ZCE_OS_WINDOWS)
    select_width = 0;
#elif defined (ZCE_OS_LINUX)
    select_width = int (handle) + 1;
#endif

    int result = ZCE_OS::select (select_width,
                                 p_set_read,
                                 p_set_write,
                                 p_set_exception,
                                 timeout_tv);

    if (0 == result )
    {
        errno = ETIMEDOUT;
        return 0;
    }

    //出现错误，
    if (0 > result )
    {
        return result;
    }

    //确定那些事件被触发了
    if ( p_set_read && FD_ISSET(handle, p_set_read) )
    {
        (*multiready_occur) |= HANDLE_READY_READ;
    }

    if ( p_set_write && FD_ISSET(handle, p_set_write))
    {
        (*multiready_occur) |= HANDLE_READY_WRITE;
    }

    if ( p_set_exception && FD_ISSET(handle, p_set_exception) )
    {
        (*multiready_occur) |= HANDLE_READY_EXCEPTION;
    }

    return result;
}

//--------------------------------------------------------------------------------------------
//因为WINdows 不支持取得socket 是否是阻塞的模式，所以Windows 下我无法先取得socket的选项，然后判断是否取消阻塞模式
//所以请你务必保证你的Socket 是阻塞模式的，否则有问题
ssize_t ZCE_OS::recvn_timeout (ZCE_SOCKET handle,
                               void *buf,
                               size_t len,
                               ZCE_Time_Value &timeout_tv,
                               int flags)
{

    ssize_t result = 0;
    bool error_occur = false;

    //如果只等待有限时间
#if defined  (ZCE_OS_WINDOWS)

    //WIN32下只能简单的打开非阻塞了
    ZCE_OS::sock_enable (handle, O_NONBLOCK);

#elif defined  (ZCE_OS_LINUX)
    //Linux简单很多，只需要对这一次发送做一些无阻塞限定就OK。
    //MSG_DONTWAIT选线，WIN32不支持
    flags |= MSG_DONTWAIT;
#endif

    int ret = 0;
    ssize_t onetime_recv = 0, bytes_recv = 0;

    //一定准备确保收到这么多字节
    for (bytes_recv = 0; static_cast<size_t>(bytes_recv) < len; bytes_recv += onetime_recv)
    {

        //等待端口准备好了处罚接收事件，这儿其实不严谨，理论，这儿timeout_tv 应该减去消耗的时间
        //LINUX的SELECT会做这件事情，但WINDOWS的不会
        ret = ZCE_OS::handle_ready (handle,
                                    &timeout_tv,
                                    HANDLE_READY_READ);

        const int HANDLE_READY_ONE = 1;

        if ( ret != HANDLE_READY_ONE )
        {
            //
            if ( 0 == ret)
            {
                errno = ETIMEDOUT;
                result = -1;
            }

            error_occur = true;
            result = ret;
            break;
        }

        //使用非阻塞端口进行接收
        onetime_recv = ZCE_OS::recv (handle,
                                     static_cast <char *> (buf) + bytes_recv,
                                     len - bytes_recv,
                                     flags);

        if (onetime_recv > 0)
        {
            continue;
        }
        //如果出现错误,== 0一般是是端口断开，==-1表示
        else
        {
            //==-1，但是表示阻塞错误，进行循环处理
            if (onetime_recv < 0 && errno == EWOULDBLOCK)
            {
                // Did select() succeed?
                onetime_recv = 0;
                continue;
            }

            //出现错误，进行处理
            error_occur = true;
            result = onetime_recv;
            break;
        }
    }

    //如果只等待有限时间，恢复原有阻塞模式
#if defined  (ZCE_OS_WINDOWS)
    ZCE_OS::sock_disable (handle, O_NONBLOCK);
#endif

    if (error_occur)
    {
        return result;
    }

    return bytes_recv;
}

//请你务必在WIN32环境保证你的Socket 是阻塞模式的，否则有问题
ssize_t ZCE_OS::sendn_timeout(ZCE_SOCKET handle,
                              const void *buf,
                              size_t len,
                              ZCE_Time_Value &timeout_tv,
                              int flags )
{

    bool error_occur = false;
    ssize_t result = 0, bytes_send = 0, onetime_send = 0;

    //如果只等待有限时间
#if defined  (ZCE_OS_WINDOWS)

    //等待一段时间要进行特殊处理
    //WIN32下只能简单的打开非阻塞了
    ZCE_OS::sock_enable (handle, O_NONBLOCK);

#elif defined  (ZCE_OS_LINUX)
    //Linux简单很多，只需要对这一次发送做一些无阻塞限定就OK。
    //MSG_DONTWAIT标志，WIN32不支持
    flags |= MSG_DONTWAIT;
#endif

    int ret = 0;

    //一定准备确保收到这么多字节
    for (bytes_send = 0; static_cast<size_t>(bytes_send) < len; bytes_send += onetime_send)
    {
        //发送在处理流程上和recv不一样，因为send往往不需要等待处理，进入select浪费
        //使用非阻塞端口进行接收，
        onetime_send = ZCE_OS::send (handle,
                                     static_cast <const char *> (buf) + bytes_send,
                                     len - bytes_send,
                                     flags);

        if (onetime_send > 0)
        {
            continue;
        }
        //如果出现错误,== 0一般是是端口断开，==-1表示错误
        else
        {
            //==-1，但是表示阻塞错误，进行循环处理
            if ( onetime_send < 0 && errno == EWOULDBLOCK)
            {
                //准备进入select
                onetime_send = 0;

                //等待端口准备好了处理发送事件，这儿其实不严谨，这儿timeout_tv 应该减去消耗的时间
                ret = ZCE_OS::handle_ready (handle,
                                            &timeout_tv,
                                            HANDLE_READY_WRITE);

                const int HANDLE_READY_ONE = 1;

                if (ret == HANDLE_READY_ONE)
                {
                    continue;
                }
                else
                {
                    //
                    if ( 0 == ret)
                    {
                        errno = ETIMEDOUT;
                        result = -1;
                    }

                    error_occur = true;
                    result = ret;
                    break;
                }
            }

            //出现错误，进行处理
            error_occur = true;
            result = onetime_send;
            break;
        }
    }

#if defined  ZCE_OS_WINDOWS
    //关闭非阻塞状态
    ZCE_OS::sock_disable (handle, O_NONBLOCK);
#endif

    //发送了错误，返回错误返回值
    if (error_occur)
    {
        return result;
    }

    return bytes_send;
}

//收UDP的数据,也带有超时处理，但是是收到多少数据就是多少了，超时用select实现
ssize_t ZCE_OS::recvfrom_timeout (ZCE_SOCKET handle,
                                  void *buf,
                                  size_t len,
                                  sockaddr *from,
                                  socklen_t *from_len,
                                  ZCE_Time_Value &timeout_tv,
                                  int flags)
{
    //如果只等待有限时间
#if defined  (ZCE_OS_WINDOWS)

    //等待一段时间要进行特殊处理
    //WIN32下只能简单的打开非阻塞了
    ZCE_OS::sock_enable (handle, O_NONBLOCK);

#elif defined  (ZCE_OS_LINUX)
    //Linux简单很多，只需要对这一次发送做一些无阻塞限定就OK。
    //MSG_DONTWAIT标志，WIN32不支持
    flags |= MSG_DONTWAIT;
#endif

    ssize_t recv_result = 0;
    int ret = ZCE_OS::handle_ready (handle,
                                    &timeout_tv,
                                    HANDLE_READY_READ);

    const int HANDLE_READY_ONE = 1;

    if (ret == HANDLE_READY_ONE)
    {
        //使用非阻塞端口进行接收
        recv_result = ZCE_OS::recvfrom (handle,
                                        static_cast <char *> (buf) ,
                                        len,
                                        flags,
                                        from,
                                        from_len);
    }
    else
    {
        //
        if (ret == 0)
        {
            errno = ETIMEDOUT;
        }

        recv_result = -1;
    }

    //如果只等待有限时间，恢复原有的状态
#if defined  ZCE_OS_WINDOWS
    ZCE_OS::sock_disable (handle, O_NONBLOCK);
#endif

    return recv_result;
}

//UDP的发送暂时是不会阻塞的，不用超时处理，写这个函数完全是为了和前面对齐
//发送UDP的数据,带超时处理参数，但是实际上进行没有超时处理
ssize_t ZCE_OS::sendto_timeout (ZCE_SOCKET handle,
                                const void *buf,
                                size_t len,
                                const sockaddr *addr,
                                int addrlen,
                                ZCE_Time_Value & /*timeout_tv*/,
                                int flags)
{
    return ZCE_OS::sendto(handle,
                          buf,
                          len,
                          flags,
                          addr,
                          addrlen
                         );
}

//--------------------------------------------------------------------------------------------
//这组函数提供仅仅为了代码测试，暂时不对外提供
//使用SO_RCVTIMEO，SO_SNDTIMEO得到一组超时处理函数

//注意SO_RCVTIMEO,SO_SNDTIMEO,只在WIN socket 2后支持
ssize_t ZCE_OS::recvn_timeout2 (ZCE_SOCKET handle,
                                void *buf,
                                size_t len,
                                ZCE_Time_Value &timeout_tv,
                                int flags)
{
    int ret = 0;

    //虽然你做了一样的外层封装，但是由于内部实现不一样，你还是要吐血。
#if defined  ZCE_OS_WINDOWS
    //超时的毫秒
    DWORD  msec_timeout = static_cast<DWORD>(timeout_tv.total_msec());
    ret = ZCE_OS::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (const void *)(&msec_timeout), sizeof(DWORD));

#elif defined  ZCE_OS_LINUX
    timeval sockopt_tv = timeout_tv;
    ret = ZCE_OS::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (const void *)(&sockopt_tv), sizeof(timeval));
#endif

    if (0 != ret )
    {
        return -1;
    }

    ssize_t result = 0, bytes_recv = 0, onetime_recv = 0;
    bool error_occur = false;

    //一定要收到len长度的字节
    for (bytes_recv = 0; static_cast<size_t>(bytes_recv) < len; bytes_recv += onetime_recv)
    {
        //使用非阻塞端口进行接收
        onetime_recv = ZCE_OS::recv (handle,
                                     static_cast <char *> (buf) + bytes_recv,
                                     len - bytes_recv,
                                     flags);

        if (onetime_recv > 0)
        {
            continue;
        }
        //如果出现错误,== 0一般是是端口断开，==-1表示
        else
        {
            //出现错误，进行处理
            error_occur = true;
            result = onetime_recv;
            break;
        }
    }

    //
    if (error_occur)
    {
        return result;
    }

    //要不要还原原来的SO_RCVTIMEO?算了，用阻塞超时调用地方应该会一直使用

    return bytes_recv;
}

//
ssize_t ZCE_OS::sendn_timeout2 (ZCE_SOCKET handle,
                                void *buf,
                                size_t len,
                                ZCE_Time_Value &timeout_tv,
                                int flags)
{

    int ret = 0;

    //虽然你做了一样的外层封装，但是由于内部实现不一样，你还是要吐血。
#if defined  ZCE_OS_WINDOWS

    DWORD  msec_timeout = static_cast<DWORD>(timeout_tv.total_msec());
    ret = ZCE_OS::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (const void *)(&msec_timeout), sizeof(DWORD));

#elif defined  ZCE_OS_LINUX
    timeval sockopt_tv = timeout_tv;
    ret = ZCE_OS::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (const void *)(&sockopt_tv), sizeof(timeval));

#endif

    if (0 != ret )
    {
        return -1;
    }

    ssize_t result = 0, bytes_send = 0, onetime_send = 0;
    bool error_occur = false;

    //一定要发送到len长度的字节
    for (bytes_send = 0; static_cast<size_t>(bytes_send) < len; bytes_send += onetime_send)
    {
        //使用非阻塞端口进行接收
        onetime_send = ZCE_OS::send (handle,
                                     static_cast <char *> (buf) + bytes_send,
                                     len - bytes_send,
                                     flags);

        //其实这儿应该调整超时时间，呵呵，偷懒了
        if (onetime_send > 0)
        {
            continue;
        }
        //如果出现错误,== 0一般是是端口断开，==-1表示
        else
        {
            //出现错误，进行处理
            error_occur = true;
            result = onetime_send;
            break;
        }
    }

    //
    if (error_occur)
    {
        return result;
    }

    return bytes_send;
}

//收UDP的数据,也带有超时处理，但是是收到多少数据就是多少了，超时用SO_RCVTIMEO实现
ssize_t ZCE_OS::recvfrom_timeout2 (ZCE_SOCKET handle,
                                   void *buf,
                                   size_t len,
                                   sockaddr *addr,
                                   socklen_t *addrlen,
                                   ZCE_Time_Value &timeout_tv,
                                   int flags)
{
    int ret = 0;
    //虽然你做了一样的外层封装，但是由于内部实现不一样，你还是要吐血。
#if defined (ZCE_OS_WINDOWS)
    
    DWORD  msec_timeout = static_cast<DWORD>(timeout_tv.total_msec());
    ret = ZCE_OS::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (const void *)(&msec_timeout), sizeof(DWORD));

#elif defined (ZCE_OS_LINUX)
    timeval sockopt_tv = timeout_tv;
    ret = ZCE_OS::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (const void *)(&sockopt_tv), sizeof(timeval));
#endif
    
    //按照socket类似函数的封装，返回-1标识失败。
    if (0 != ret)
    {
        return -1;
    }

    //使用非阻塞端口进行接收
    ssize_t recv_result = ZCE_OS::recvfrom (handle,
                                            static_cast <char *> (buf) ,
                                            len,
                                            flags,
                                            addr,
                                            addrlen);

    return recv_result;
}

//UDP的发送暂时是不会阻塞的，不用超时处理，写这个函数完全是为了和前面对齐
//发送UDP的数据,带超时处理参数，但是实际上进行没有超时处理
ssize_t ZCE_OS::sendto_timeout2 (ZCE_SOCKET handle,
                                 const void *buf,
                                 size_t len,
                                 const sockaddr *addr,
                                 int addrlen,
                                 ZCE_Time_Value & /*timeout_tv*/,
                                 int flags)
{
    return ZCE_OS::sendto(handle,
                          buf,
                          len,
                          flags,
                          addr,
                          addrlen
                         );
}

//--------------------------------------------------------------------------------------------

//转换字符串到网络地址，第一个参数af是地址族，转换后存在dst中，
//注意这个函数return 1标识成功，return 负数标识错误，return 0标识格式匹配错误
int ZCE_OS::inet_pton (int family,
                       const char *strptr,
                       void *addrptr)
{
#if defined (ZCE_OS_WINDOWS)

    //为什么不让我用inet_pton ,(Vista才支持),不打开下面注释的原因是，编译会通过了，但你也没法用,XP和WINSERVER2003都无法使用，
    //VISTA,WINSERVER2008的_WIN32_WINNT都是0x0600
    //#if defined (_WIN32_WINNT) && _WIN32_WINNT >=  0x0600
    //    return ::inet_pton(family,strptr,addrptr);
    //#else

    //sscanf取得的域的个数
    int get_fields_num = 0;

    if (  AF_INET == family )
    {
        struct in_addr *in_val = reinterpret_cast<in_addr *>(addrptr);

        //为什么不直接用in_val->S_un.S_un_b.s_b1？你猜
        const int NUM_FIELDS_AF_INET = 4;
        uint32_t u[NUM_FIELDS_AF_INET] = {0};
        get_fields_num = sscanf(strptr,
                                "%u.%u.%u.%u",
                                &(u[0]),
                                &(u[1]),
                                &(u[2]),
                                &(u[3])
                               );

        //输入的字符串不合乎标准

        if ( NUM_FIELDS_AF_INET != get_fields_num || u[0] > 0xFF || u[1] > 0xFF || u[2] > 0xFF || u[3] > 0xFF )
        {
            return 0;
        }

        //读取了4个域,
        //这个转换只能在WINDOWS下用（这些union只有WINDOWS下有定义），如果要通用，要改代码。
        in_val->S_un.S_un_b.s_b1 = static_cast<unsigned char> (u[0]);
        in_val->S_un.S_un_b.s_b2 = static_cast<unsigned char> (u[1]);
        in_val->S_un.S_un_b.s_b3 = static_cast<unsigned char> (u[2]);
        in_val->S_un.S_un_b.s_b4 = static_cast<unsigned char> (u[3]);

        //注意，返回1标识成功
        return (1);
    }
    else if ( AF_INET6 == family )
    {
        //fucnking 创造RFC1884的哥们，你就考虑简化，也考虑一下写代码的人如何进行转换把。送你一只草泥马
        //RFC1884 对不起，我不支持IPV6的缩略格式和IPV4映射格式，那个太太麻烦了。
        //IPV6的0缩略格式包括 '::'在开头的::FFFF:A:B  '::'在中间的 A:B:::C(你不知道有几个0被省略了)，'::'在尾巴上的
        //IPV4映射成IPV6的格式可以标识成::FFFF:A.B.C.D

        //输入的字符串不合乎标准
        const int NUM_FIELDS_AF_INET6 = 8;

        const char INET6_STR_UP_CHAR[] = {"1234567890ABCDEF"};

        //先请0
        memset(addrptr, 0, sizeof(in_addr6));
        struct in_addr6 *in_val6 = reinterpret_cast<in_addr6 *>(addrptr);

        size_t in_str_len = strlen(strptr);
        //前一个字符是否是冒号
        bool pre_char_colon = false;
        //字符串是否
        bool str_abbreviate = false;
        //
        int havedot_ipv4_mapped = 0;

        //
        size_t word_start_pos = 0;

        uint16_t for_word[NUM_FIELDS_AF_INET6] = {0};
        uint16_t back_word[NUM_FIELDS_AF_INET6] = {0};

        size_t forword_num = 0, backword_num = 0;

        for (size_t i = 0; i <= in_str_len; ++i)
        {
            //
            if (':' == strptr [i] )
            {
                //如果后面的字符也是：，标识是缩写格式
                if ( pre_char_colon == true  )
                {
                    //如果没有发生过缩写
                    if (false == str_abbreviate)
                    {
                        str_abbreviate = true;
                    }
                    //不可能发送两次缩写，
                    else
                    {
                        return 0;
                    }

                    continue;
                }

                //.出现后，不可能出现：，格式错误
                if (havedot_ipv4_mapped > 0)
                {
                    return 0;
                }

                //不可能出现8个冒号
                if (backword_num + forword_num >= NUM_FIELDS_AF_INET6)
                {
                    return 0;
                }

                //如果已经有缩写，那么记录到后向数据队列中
                if (str_abbreviate)
                {
                    get_fields_num = sscanf(strptr + word_start_pos, "%hx:",  &(back_word[backword_num]));
                    ++backword_num;
                }
                else
                {
                    get_fields_num = sscanf(strptr + word_start_pos, "%hx:",  &(for_word[forword_num]));
                    ++forword_num;
                }

                pre_char_colon = true;
                continue;
            }
            //IPV4映射IPV6的写法
            else if ( '.' == strptr [i] )
            {
                //如果前面是:,错误
                if (pre_char_colon)
                {
                    return 0;
                }

                ++havedot_ipv4_mapped;
            }
            else
            {
                //出现其他字符，认为错误，滚蛋，
                if ( NULL == strchr(INET6_STR_UP_CHAR, toupper(strptr [i])) )
                {
                    return 0;
                }

                //如果前面一个是:
                if (pre_char_colon)
                {
                    pre_char_colon = false;
                    word_start_pos = i;
                }

                continue;
            }
        }

        //对最后一个WORD或者2个WORD进行处理

        //出现了.，并且出现了了3次，havedot_ipv4_mapped ，
        if (havedot_ipv4_mapped > 0)
        {
            const int NUM_FIELDS_AF_INET = 4;
            uint32_t u[NUM_FIELDS_AF_INET] = {0};
            get_fields_num = sscanf(strptr + word_start_pos,
                                    "%u.%u.%u.%u",
                                    &(u[0]),
                                    &(u[1]),
                                    &(u[2]),
                                    &(u[3])
                                   );

            //输入的字符串不合乎标准
            if ( NUM_FIELDS_AF_INET != get_fields_num || u[0] > 0xFF || u[1] > 0xFF || u[2] > 0xFF || u[3] > 0xFF )
            {
                return 0;
            }

            back_word[backword_num] = static_cast<uint16_t>( u[0] << 8 | u[1]);
            ++backword_num;
            back_word[backword_num] = static_cast<uint16_t>( u[2] << 8 | u[3]);
            ++backword_num;
        }
        else
        {
            if (false == pre_char_colon)
            {
                sscanf(strptr + word_start_pos , "%hx",  &(back_word[backword_num]));
                ++backword_num;
            }
        }

        //对每一个WORD进行赋值，前赋值前向的，再赋值后向的,中间的如果被省略就是0了。不管了
        //这个转换只能在WINDOWS下用（这些union只有WINDOWS下有定义），如果要通用，要改代码。
        for (size_t k = 0; k < forword_num; ++k)
        {
            in_val6->u.Word[k] = htons( for_word[k]);
        }

        for (size_t k = 0; k < backword_num; ++k)
        {
            in_val6->u.Word[NUM_FIELDS_AF_INET6 - backword_num + k] = htons( back_word[k]);
        }

        //返回1标识成功
        return (1);
    }
    //不支持
    else
    {
        errno = EAFNOSUPPORT;
        return 0;
    }

#elif defined (ZCE_OS_LINUX)
    //LINuX下有这个函数
    return ::inet_pton(family, strptr, addrptr);
#endif
}

//函数原型如下[将“点分十进制” －> “整数”],IPV6将，:分割16进制转换成128位数字
const char *ZCE_OS::inet_ntop (int family,
                               const void *addrptr,
                               char *strptr,
                               size_t len)
{

#if defined (ZCE_OS_WINDOWS)

    //根据不同的协议簇进行不同的处理
    if (  AF_INET == family )
    {
        const struct in_addr *in_val = reinterpret_cast<const in_addr *>(addrptr);
        int ret_len = snprintf(strptr,
                               len,
                               "%u.%u.%u.%u",
                               in_val->S_un.S_un_b.s_b1,
                               in_val->S_un.S_un_b.s_b2,
                               in_val->S_un.S_un_b.s_b3,
                               in_val->S_un.S_un_b.s_b4);

        //格式化字符串失败
        if (ret_len > static_cast<int>(len) || ret_len <= 0 )
        {
            errno = ENOSPC;
            return NULL;
        }

        return strptr;
    }
    else if ( AF_INET6 == family )
    {
        //对不起，我只支持转换成IPV6的标准格式，不支持转换成IPV6的缩略格式和IPV4映射格式，

        const struct in_addr6 *in_val6 = reinterpret_cast<const in_addr6 *>(addrptr);

        const int NUM_FIELDS_AF_INET6 = 8;
        uint16_t u[NUM_FIELDS_AF_INET6];

        //因为是short，还是要转换成本地序列
        u[0] = ::ntohs(in_val6->u.Word[0]);
        u[1] = ::ntohs(in_val6->u.Word[1]);
        u[2] = ::ntohs(in_val6->u.Word[2]);
        u[3] = ::ntohs(in_val6->u.Word[3]);
        u[4] = ::ntohs(in_val6->u.Word[4]);
        u[5] = ::ntohs(in_val6->u.Word[5]);
        u[6] = ::ntohs(in_val6->u.Word[6]);
        u[7] = ::ntohs(in_val6->u.Word[7]);

        int ret_len = snprintf(strptr,
                               len,
                               "%hx:%hx:%hx:%hx:%hx:%hx:%hx:%hx",
                               u[0],
                               u[1],
                               u[2],
                               u[3],
                               u[4],
                               u[5],
                               u[6],
                               u[7]
                              );

        if (ret_len > static_cast<int>(len) || ret_len <= 0 )
        {
            errno = ENOSPC;
            return NULL;
        }

        return strptr;
    }
    else
    {
        errno = EAFNOSUPPORT;
        return NULL;
    }

#elif defined (ZCE_OS_LINUX)
    //LINuX下有这个函数
    return ::inet_ntop(family, addrptr, strptr, len);
#endif
}

//输出IP地址信息，内部是不使用静态变量，线程安全，BUF长度IPV4至少长度>15.IPV6至少长度>39
const char *ZCE_OS::socketaddr_ntop(const sockaddr *sock_addr,
                                    char *str_ptr,
                                    size_t str_len)
{
    //根据不同的地址协议族，进行转换
    if (sock_addr->sa_family == AF_INET)
    {
        const sockaddr_in *sockadd_ipv4 = reinterpret_cast<const sockaddr_in *>(sock_addr);
        return ZCE_OS::inet_ntop(AF_INET,
                                 (void *)(&(sockadd_ipv4->sin_addr)),
                                 str_ptr,
                                 str_len);
    }
    else if (sock_addr->sa_family == AF_INET6)
    {
        const sockaddr_in6 *sockadd_ipv6 = reinterpret_cast<const sockaddr_in6 *>(sock_addr);
        return ZCE_OS::inet_ntop(AF_INET6,
                                 (void *)(&(sockadd_ipv6->sin6_addr)),
                                 str_ptr, str_len);
    }
    else
    {
        errno = EAFNOSUPPORT;
        return NULL;
    }
}

//输出IP地址信息以及端口信息，内部是不使用静态变量，线程安全，BUF长度IPV4至少长度>21.IPV6至少长度>45
const char *ZCE_OS::socketaddr_ntop_ex(const sockaddr *sock_addr,
                                       char *str_ptr,
                                       size_t str_len)
{
    uint16_t addr_port = 0;
    const char *ret_str = NULL;

    //根据不同的地址协议族，进行转换，不使用上面那个函数的原因是因为，我同时要进行读取port的操作
    if (sock_addr->sa_family == AF_INET)
    {
        const sockaddr_in *sockadd_ipv4 = reinterpret_cast<const sockaddr_in *>(sock_addr);
        addr_port = ntohs(sockadd_ipv4->sin_port);
        ret_str = ZCE_OS::inet_ntop(AF_INET,
                                    (void *)(&(sockadd_ipv4->sin_addr)),
                                    str_ptr,
                                    str_len);
    }
    else if (sock_addr->sa_family == AF_INET6)
    {
        const sockaddr_in6 *sockadd_ipv6 = reinterpret_cast<const sockaddr_in6 *>(sock_addr);
        addr_port = ntohs(sockadd_ipv6->sin6_port);
        ret_str = ZCE_OS::inet_ntop(AF_INET6,
                                    (void *)(&(sockadd_ipv6->sin6_addr)),
                                    str_ptr, str_len);
    }
    else
    {
        errno = EAFNOSUPPORT;
        return NULL;
    }

    //如果返回错误
    if ( NULL == ret_str )
    {
        return NULL;
    }

    size_t add_str_len = strlen(str_ptr);

    //5个数字，一个连接符，一个空字符标志
    const size_t PORT_LEN = 7;

    if (str_len < add_str_len + PORT_LEN)
    {
        return NULL;
    }

    //前面已经检查过了，这儿不判断返回了
    snprintf(str_ptr + add_str_len, str_len - add_str_len, "#%u", addr_port);

    return str_ptr;
}

/*
我们对企业网的IP分配一般以RFC1918中定义的非Internet连接的网络地址，
也称为私有地址。由Internet地址授权机构（IANA）控制的IP地址分配方案中，
留出了三类网络地址，给不连到Internet上的专用网使用。它分别是：
A类：10.0.0.0 ~ 10.255.255.255；
B类：172.16.0.0 ~ 172.31.255.255；
C类：192.168.0.0 ~ 192.168.255.255。
其中的一个私有地址网段是：192.168.0.0是我们在内网IP分配中最常用的网段。
IANA保证这些网络号不会分配给连到Internet上的任何网络，
因此任何人都可以自由地选择这些网络地址作为自己的私有网络地址。
在申请的合法IP不足的情况下，企业网内网可以采用私有IP地址的网络地址分配方案；
企业网外网接入、DMZ区使用合法IP地址。
如果是全0，也是自己内部ip <==== 这个是那个同学加的，这个在某种程度上是对的，因为0只会出现在本机的判定上
*/

#if !defined ZCE_IS_INTERNAL
#define ZCE_IS_INTERNAL(ip_addr)   ((ip_addr >= 0x0A000000 && ip_addr <= 0x0AFFFFFF ) ||  \
(ip_addr >= 0xAC100000 && ip_addr <= 0xAC1FFFFF) ||  \
(ip_addr >= 0xC0A80000 && ip_addr <= 0xC0A8FFFF) ||  \
(ip_addr == INADDR_ANY))
#endif 

//检测一个地址是否是内网地址
bool ZCE_OS::is_internal(const sockaddr_in *sock_addr_ipv4)
{
    uint32_t ip_addr = ZCE_OS::get_ip_address(sock_addr_ipv4);

    //检查3类地址
    if (ZCE_IS_INTERNAL(ip_addr))
    {
        return true;
    }
    return false;
}

bool ZCE_OS::is_internal(uint32_t ipv4_addr_val)
{
    //检查3类地址
    if (ZCE_IS_INTERNAL(ipv4_addr_val))
    {
        return true;
    }
    return false;
}



//-------------------------------------------------------------------------------------
//域名解析，转换IP地址的几个函数

//通过域名得到相关的IP地址
hostent *ZCE_OS::gethostbyname(const char *hostname)
{
    return ::gethostbyname(hostname);
}

// GNU extensions
hostent *ZCE_OS::gethostbyname2(const char *hostname,
                                int af)
{
#if defined (ZCE_OS_WINDOWS)

    hostent *hostent_ptr = ::gethostbyname(hostname);

    //
    if (hostent_ptr->h_addrtype != af)
    {
        return NULL;
    }

    return hostent_ptr;

#elif defined (ZCE_OS_LINUX)
    return ::gethostbyname2(hostname, af);
#endif
}

//非标准函数,得到某个域名的IPV4的地址信息，但是使用起来比较容易和方便
//name 域名
//uint16_t service_port 端口号，本地序
//ary_addr_num  ,输入输出参数，输入标识ary_sock_addr的个数，输出时标识返回的队列数量
//ary_sock_addr ,输出参数，返回的地址队列
int ZCE_OS::gethostbyname_inary(const char *hostname,
                                uint16_t service_port,
                                size_t *ary_addr_num,
                                sockaddr_in ary_sock_addr[])
{
    //其实这个函数没法重入
    struct hostent *hostent_ptr = ::gethostbyname(hostname);

    if (!hostent_ptr)
    {
        return -1;
    }

    if (hostent_ptr->h_addrtype != AF_INET)
    {
        errno = EINVAL;
        return -1;
    }

    //检查返回
    ZCE_ASSERT(hostent_ptr->h_length == sizeof(in_addr));

    //循环得到所有的IP地址信息
    size_t i = 0;
    char **addr_pptr = hostent_ptr->h_addr_list;

    for (; (i < *ary_addr_num) && (*addr_pptr != NULL); addr_pptr++, ++i)
    {
        ary_sock_addr[i].sin_family = AF_INET;
        //本来就是网络序
        memcpy(&(ary_sock_addr[i].sin_addr), addr_pptr, hostent_ptr->h_length);
        //端口转换成网络序
        ary_sock_addr[i].sin_port = htons(service_port);
    }

    //记录数量
    *ary_addr_num = i;

    return 0;
}

//非标准函数,得到某个域名的IPV6的地址信息，但是使用起来比较容易和方便
int ZCE_OS::gethostbyname_in6ary(const char *hostname,
                                 uint16_t service_port,
                                 size_t *ary_addr6_num,
                                 sockaddr_in6 ary_sock_addr6[])
{
    //其实这个函数没法重入
    struct hostent *hostent_ptr = ::gethostbyname(hostname);

    if (!hostent_ptr)
    {
        return -1;
    }

    if (hostent_ptr->h_addrtype != AF_INET6)
    {
        errno = EINVAL;
        return -1;
    }

    //检查返回的地址实习是不是IPV6的
    ZCE_ASSERT(hostent_ptr->h_length == sizeof(in6_addr));

    //循环得到所有的IP地址信息
    size_t i = 0;
    char **addr_pptr = hostent_ptr->h_addr_list;

    for (; (i < *ary_addr6_num) && (*addr_pptr != NULL); addr_pptr++, ++i)
    {
        ary_sock_addr6[i].sin6_family = AF_INET6;
        //本来就是网络序
        memcpy(&(ary_sock_addr6[i].sin6_addr), addr_pptr, hostent_ptr->h_length);
        //端口转换成网络序
        ary_sock_addr6[i].sin6_port = htons(service_port);
    }

    //记录数量
    *ary_addr6_num = i;

    return 0;
}

//根据地址得到域名的函数,推荐使用替代函数getnameinfo ,
hostent *ZCE_OS::gethostbyaddr(const void *addr,
                               socklen_t len,
                               int family)
{
    return ::gethostbyaddr((const char *)addr, len, family);
};

//非标准函数，通过IPV4地址取得域名
int ZCE_OS::gethostbyaddr_in(const sockaddr_in *sock_addr,
                             char *host_name,
                             size_t name_len)
{
    struct hostent *hostent_ptr = ZCE_OS::gethostbyaddr(sock_addr,
                                                        sizeof(sockaddr_in),
                                                        sock_addr->sin_family
                                                       );

    //如果返回失败
    if (!hostent_ptr )
    {
        return -1;
    }

    strncpy(host_name, hostent_ptr->h_name, name_len);

    return 0;
}

//非标准函数，通过IPV6地址取得域名
int ZCE_OS::gethostbyaddr_in6(const sockaddr_in6 *sock_addr6,
                              char *host_name,
                              size_t name_len)
{

    struct hostent *hostent_ptr = ZCE_OS::gethostbyaddr(sock_addr6,
                                                        sizeof(sockaddr_in6),
                                                        sock_addr6->sin6_family
                                                       );

    //如果返回失败
    if (!hostent_ptr )
    {
        return -1;
    }

    strncpy(host_name, hostent_ptr->h_name, name_len);

    return 0;
}

//通过域名得到服务器地址信息，可以同时得到IPV4，和IPV6的地址
//hints 参数说明，
//如果要同时得到IPV4，IPV6的地址，那么hints.ai_family =  AF_UNSPEC
//ai_socktype参数最好还是填写一个值，否则可能返回SOCK_DGRAM,SOCK_STREAM各一个，
//ai_flags 填0一般就OK，AI_CANONNAME表示返回的result的第一个节点会有ai_canoname参数，AI_PASSIVE在参数hostname为NULL时，让IP地址信息返回0，（蛋疼的一个参数）
//ai_protocol，填0把
int ZCE_OS::getaddrinfo( const char *hostname,
                         const char *service,
                         const addrinfo *hints,
                         addrinfo **result )
{
    return ::getaddrinfo(hostname, service, hints, result);
}

//释放getaddrinfo得到的结果
void ZCE_OS::freeaddrinfo(struct addrinfo *result)
{
    return ::freeaddrinfo(result);
}

//非标准函数,得到某个域名的IPV4的地址数组，使用起来比较容易和方便
int ZCE_OS::getaddrinfo_inary(const char *hostname,
                              uint16_t service_port,
                              size_t *ary_addr_num,
                              sockaddr_in ary_sock_addr[])
{

    int ret = 0;
    addrinfo hints, *result = NULL;

    memset(&hints, 0, sizeof(addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_CANONNAME;
    ret = ZCE_OS::getaddrinfo(hostname,
                              NULL,
                              &hints,
                              &result);

    if (ret != 0)
    {
        return ret;
    }

    if (!result)
    {
        errno = EINVAL;
        return -1;
    }

    //检查返回的地址实习是不是IPV4的
    ZCE_ASSERT(result->ai_addrlen == sizeof(sockaddr_in));

    //循环得到所有的IP地址信息
    size_t i = 0;
    addrinfo *prc_node = result;

    for (; (i < *ary_addr_num) && (prc_node != NULL); prc_node = prc_node->ai_next, ++i)
    {
        memcpy(&(ary_sock_addr[i]), prc_node->ai_addr, prc_node->ai_addrlen);
        //端口转换成网络序
        ary_sock_addr[i].sin_port = htons(service_port);
    }

    //记录数量
    *ary_addr_num = i;

    //释放空间
    ZCE_OS::freeaddrinfo(result);

    return 0;
}

//非标准函数,得到某个域名的IPV6的地址数组，使用起来比较容易和方便
int ZCE_OS::getaddrinfo_in6ary(const char *hostname,
                               uint16_t service_port,
                               size_t *ary_addr6_num,
                               sockaddr_in6 ary_sock_addr6[])
{
    int ret = 0;
    addrinfo hints, *result = NULL;

    memset(&hints, 0, sizeof(addrinfo));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_CANONNAME;
    ret = ZCE_OS::getaddrinfo(hostname,
                              NULL,
                              &hints,
                              &result);

    if (ret != 0)
    {
        return ret;
    }

    if (!result)
    {
        errno = EINVAL;
        return -1;
    }

    //检查返回的地址实习是不是IPV4的
    ZCE_ASSERT(result->ai_addrlen == sizeof(sockaddr_in6));

    //循环得到所有的IP地址信息
    size_t i = 0;
    addrinfo *prc_node = result;

    for (; (i < *ary_addr6_num) && (prc_node != NULL); prc_node = prc_node->ai_next, ++i)
    {
        memcpy(&(ary_sock_addr6[i]), prc_node->ai_addr, prc_node->ai_addrlen);
        //端口转换成网络序
        ary_sock_addr6[i].sin6_port = htons(service_port);
    }

    //记录数量
    *ary_addr6_num = i;

    //释放空间
    ZCE_OS::freeaddrinfo(result);

    return 0;
}

//通过IP地址信息，反查域名.服务名，可以重入函数
int ZCE_OS::getnameinfo(const struct sockaddr *sa,
                        socklen_t salen,
                        char *host,
                        size_t hostlen,
                        char *serv,
                        size_t servlen,
                        int flags)
{
#if defined (ZCE_OS_WINDOWS)
    return ::getnameinfo(sa, salen, host, static_cast<DWORD>(hostlen), serv, static_cast<DWORD>(servlen), flags);
#elif defined (ZCE_OS_LINUX)
    return ::getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
#endif
}

//非标准函数，通过IPV4地址取得域名
int ZCE_OS::getnameinfo_in(const sockaddr_in *sock_addr,
                           char *host_name,
                           size_t name_len)
{
    return ZCE_OS::getnameinfo(reinterpret_cast<const sockaddr *>(sock_addr),
                               sizeof(sockaddr_in),
                               host_name,
                               name_len,
                               NULL,
                               0,
                               NI_NAMEREQD);
}

//非标准函数，通过IPV6地址取得域名
int ZCE_OS::getnameinfo_in6(const sockaddr_in6 *sock_addr6,
                            char *host_name,
                            size_t name_len)
{
    return ZCE_OS::getnameinfo(reinterpret_cast<const sockaddr *>(sock_addr6),
                               sizeof(sockaddr_in6),
                               host_name,
                               name_len,
                               NULL,
                               0,
                               NI_NAMEREQD);
}

//-------------------------------------------------------------------------------------
//IPV4和IPV6之间相互转换的函数，都是非标准函数，

//将一个IPV4的地址映射为IPV6的地址
int ZCE_OS::inaddr_map_inaddr6(const in_addr *src, in6_addr *dst)
{
    //清0
    memset(dst, 0, sizeof(in6_addr));

    //
    memcpy(reinterpret_cast<char *>(dst) + sizeof(in6_addr) - sizeof(in_addr),
           reinterpret_cast<const char *>(src),
           sizeof(in_addr));

    //映射地址的倒数第3个WORD为0xFFFF
    dst->s6_addr[10] = 0xFF;
    dst->s6_addr[11] = 0xFF;

    return 0;
}

//将一个IPV4的Sock地址映射为IPV6的地址
int ZCE_OS::sockin_map_sockin6(const sockaddr_in *src, sockaddr_in6 *dst)
{
    return ZCE_OS::inaddr_map_inaddr6(&(src->sin_addr),
                                      &(dst->sin6_addr));
}

//判断一个地址是否是IPV4映射的地址
bool ZCE_OS::is_in6_addr_v4mapped(const in6_addr *in6)
{
    //这样把映射地址和兼容地址都判断了。据说兼容地址以后会被淘汰
    if (in6->s6_addr[0] == 0
        && in6->s6_addr[1] == 0
        && in6->s6_addr[2] == 0
        && in6->s6_addr[3] == 0
        && in6->s6_addr[4] == 0
        && in6->s6_addr[5] == 0
        && in6->s6_addr[6] == 0
        && in6->s6_addr[7] == 0
        && in6->s6_addr[8] == 0
        && in6->s6_addr[9] == 0
        && ((in6->s6_addr[10] == 0xFF && in6->s6_addr[11] == 0xFF) || (in6->s6_addr[10] == 0 && in6->s6_addr[11] == 0))
       )
    {
        return true;
    }

    return false;
}

//如果一个IPV6的地址从IPV4映射过来的，转换回IPV4的地址
int ZCE_OS::mapped_in6_to_in(const in6_addr *src, in_addr *dst)
{
    //先检查是否是映射的地址
    if ( false == ZCE_OS::is_in6_addr_v4mapped(src) )
    {
        errno = EINVAL;
        return -1;
    }

    memcpy(reinterpret_cast<char *>(dst) ,
           reinterpret_cast<const char *>(src) + sizeof(in6_addr) - sizeof(in_addr),
           sizeof(in_addr));
    return 0;
}
//如果一个IPV6的socketaddr_in6地址从IPV4映射过来的，转换回IPV4的socketaddr_in地址
int ZCE_OS::mapped_sockin6_to_sockin(const sockaddr_in6 *src, sockaddr_in *dst)
{
    return ZCE_OS::mapped_in6_to_in(&(src->sin6_addr),
                                    &(dst->sin_addr));
}

