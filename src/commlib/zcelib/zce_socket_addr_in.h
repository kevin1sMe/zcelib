#ifndef ZCE_LIB_SOCKET_ADDR_IN_
#define ZCE_LIB_SOCKET_ADDR_IN_

#include "zce_socket_addr_base.h"

//IPv4的物理结构
class ZCELIB_EXPORT ZCE_Sockaddr_In : public ZCE_Sockaddr
{
public:

    //默认构造函数
    ZCE_Sockaddr_In (void);

    //根据sockaddr_in构造，
    ZCE_Sockaddr_In (const sockaddr_in *addr);

    //根据IP地址(XXX.XXX.XXX.XXX)字符串，端口号初始化构造
    ZCE_Sockaddr_In (const char *ip_addr_str,
                     uint16_t port_number);

    //根据地址(整数)，端口号初始化构造
    ZCE_Sockaddr_In (uint32_t ip_addr,
                     uint16_t port_number);

    //拷贝构造，一定要写，这个类的基类指针是指向自己的一个地址的，
    ZCE_Sockaddr_In (const ZCE_Sockaddr_In &others);

    ~ZCE_Sockaddr_In();

public:

    //设置地址信息
    virtual  void set_sockaddr (sockaddr *addr, socklen_t len);

    //根据IP地址(XXX.XXX.XXX.XXX)字符串，端口号设置
    int set(const char ip_addr_str[16],
            uint16_t port_number);
    //根据地址IP整数，端口号设置
    int set(uint32_t ip_addr,
            uint16_t port_number);

    //设置端口好，
    inline void set_port_number (uint16_t);
    //取得端口号
    inline uint16_t get_port_number (void) const;

    //取得以.符号的IP地址信息STRING
    inline const char *get_host_addr (char *addr, int addr_size) const;
    //不可冲入的非安全函数
    inline const char *get_host_addr (void) const;

    //取得以.符号的IP地址#端口号STRING
    const char *get_host_addr_port(char *addr, int addr_size) const;
    //不可冲入的非安全函数
    //const char *get_host_addr_port() const;

    //取得IP地址，本地序列
    inline uint32_t get_ip_address (void) const;

    //检查地址是否是一个内网地址
    bool is_internal();

    //检查地址是否是一个外网地址，其实我就简单认为不是外网地址就是内网地址
    bool is_internet();

    //比较两个地址是否相等
    bool operator == (const ZCE_Sockaddr_In &others) const;
    //比较两个地址是否不想等
    bool operator != (const ZCE_Sockaddr_In &others) const;

    //检查IP地址是否相等,忽视端口
    bool is_ip_equal (const ZCE_Sockaddr_In &others) const;

    //DNS相关函数，
    //取得IP地址相关的域名信息,调用函数是getnameinfo
    int get_name_info(char *host_name, size_t name_len) const;

    //取得域名相关的IP地址信息，调用函数是getaddrinfo
    int get_addr_info(const char *hostname, uint16_t service_port = 0);

    //各种操作符号转换函数，方便各种使用，让ZCE_Sockaddr_In的行为和sockaddr_in基本一致
    //返回sockaddr_in
    operator sockaddr_in () const;
    //返回内部const sockaddr_in的指针，（不可以被修改）
    operator const sockaddr_in *() const;
    //返回内部sockaddr_in的指针，（可以被修改）
    operator sockaddr_in *();

protected:

    //IPV4的地址
    sockaddr_in           in4_addr_;
};

//取得IP地址，本地序列
inline uint32_t ZCE_Sockaddr_In::get_ip_address (void) const
{
    return ntohl(in4_addr_.sin_addr.s_addr);
}

//设置端口好，
inline void ZCE_Sockaddr_In::set_port_number (uint16_t port_number)
{
    in4_addr_.sin_port = ntohs(port_number);
}
//取得端口号
inline uint16_t ZCE_Sockaddr_In::get_port_number (void) const
{
    return ntohs(in4_addr_.sin_port);
}

//取得以.符号的IP地址信息STRING
inline const char *ZCE_Sockaddr_In::get_host_addr (char *addr_buf, int addr_size) const
{
    return ZCE_OS::socketaddr_ntop(reinterpret_cast<const sockaddr *>(&in4_addr_),
                                   addr_buf,
                                   addr_size);
}

//不可冲入的非安全函数
inline const char *ZCE_Sockaddr_In::get_host_addr (void) const
{
    const size_t BUF_LEN = 64;
    static char in4_buf[BUF_LEN + 1];
    in4_buf[BUF_LEN] = '\0';

    return ZCE_OS::socketaddr_ntop(reinterpret_cast<const sockaddr *>(&in4_addr_),
                                   in4_buf,
                                   BUF_LEN);
}

//取得以.符号的IP地址#端口号STRING
inline const char *ZCE_Sockaddr_In::get_host_addr_port(char *addr_buf, int addr_size) const
{
    return ZCE_OS::socketaddr_ntop_ex(reinterpret_cast<const sockaddr *>(&in4_addr_),
                                      addr_buf,
                                      addr_size);
}

#endif  //ZCE_LIB_SOCKET_ADDR_IN_

