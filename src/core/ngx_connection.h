
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CONNECTION_H_INCLUDED_
#define _NGX_CONNECTION_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_listening_s  ngx_listening_t;

// ngx_listening_t 表示着 Nginx 的一个监听端口，通常会有 2 个，一个是 80 端口，另一个则是 443 端口
struct ngx_listening_s {

    ngx_socket_t        fd;                   // 监听套接字的 socket fd

    struct sockaddr    *sockaddr;             // 监听套接字的监听地址
    socklen_t           socklen;              // sockaddr 的长度
    size_t              addr_text_max_len;

    ngx_str_t           addr_text;            // 字符串形式所保存的 ip 地址

    int                 type;                 // 监听套接字类型，通常为 SOCK_STREAM

    int                 backlog;              // 已完成连接队列的最大长度

    int                 rcvbuf;               // socket 接收缓冲区的大小
    int                 sndbuf;               // socket 发送缓冲区的大小
#if (NGX_HAVE_KEEPALIVE_TUNABLE)
    int                 keepidle;             // TCP Keep-Alive 选项
    int                 keepintvl;
    int                 keepcnt;
#endif

    /* handler of accepted connection */
    // 连接建立时的回调函数，本质上是一个泛型函数，也就是不同的事件将会有不同的回调函数，epoll 的本质其实就是一个又一个的回调
    ngx_connection_handler_pt   handler;

    void               *servers;  /* array of ngx_http_in_addr_t, for example */

    ngx_log_t           log;
    ngx_log_t          *logp;

    size_t              pool_size;             // 内存池大小
    /* should be here because of the AcceptEx() preread */
    // TODO: 暂时不知道这个参数是干嘛的，后续再填坑
    size_t              post_accept_buffer_size;

    // 从这里就可以看出 nginx 使用单链表来保存多个监听端口，不太理解这种做法，使用一个静态数组不就好了?
    ngx_listening_t    *previous;

    // ngx_connection_t 可以好好说道说道，ngx_connection_t 表示一个 TCP 连接，或者说一个感兴趣的事件，不管是监听套接字，还是连接套接字，都会
    // 使用 ngx_connection_t 来进行表示，这里面保存了相当丰富的信息，比如对端的 IP 地址、端口号，所接收的数据，等等等等。该结构与 nginx epoll 模型
    // 有着千丝万缕的关系，同样是 nginx 的核心结构之一，定义在 ngx_core.h 中，从文件名就可以看出其重要性了
    ngx_connection_t   *connection;

    // 红黑树的结构，暂时也不知道是干啥的
    ngx_rbtree_t        rbtree;
    ngx_rbtree_node_t   sentinel;

    ngx_uint_t          worker;

    unsigned            open:1;
    unsigned            remain:1;
    unsigned            ignore:1;

    unsigned            bound:1;       /* already bound */
    unsigned            inherited:1;   /* inherited from previous process */
    unsigned            nonblocking_accept:1;
    unsigned            listen:1;
    unsigned            nonblocking:1;
    unsigned            shared:1;    /* shared between threads or processes */
    unsigned            addr_ntop:1;
    unsigned            wildcard:1;

#if (NGX_HAVE_INET6)
    unsigned            ipv6only:1;
#endif
    unsigned            reuseport:1;
    unsigned            add_reuseport:1;
    unsigned            keepalive:2;

    unsigned            deferred_accept:1;
    unsigned            delete_deferred:1;
    unsigned            add_deferred:1;
#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    char               *accept_filter;
#endif
#if (NGX_HAVE_SETFIB)
    int                 setfib;
#endif

#if (NGX_HAVE_TCP_FASTOPEN)
    int                 fastopen;
#endif

};


typedef enum {
    NGX_ERROR_ALERT = 0,
    NGX_ERROR_ERR,
    NGX_ERROR_INFO,
    NGX_ERROR_IGNORE_ECONNRESET,
    NGX_ERROR_IGNORE_EINVAL
} ngx_connection_log_error_e;


typedef enum {
    NGX_TCP_NODELAY_UNSET = 0,
    NGX_TCP_NODELAY_SET,
    NGX_TCP_NODELAY_DISABLED
} ngx_connection_tcp_nodelay_e;


typedef enum {
    NGX_TCP_NOPUSH_UNSET = 0,
    NGX_TCP_NOPUSH_SET,
    NGX_TCP_NOPUSH_DISABLED
} ngx_connection_tcp_nopush_e;


#define NGX_LOWLEVEL_BUFFERED  0x0f
#define NGX_SSL_BUFFERED       0x01
#define NGX_HTTP_V2_BUFFERED   0x02


struct ngx_connection_s {

    /*
     * 在 nginx 初始化时，data 指针充当 TCP 连接池中节点的指针，也就是说，data 将指向下一个 ngx_connection_t 对象
     * 当连接被使用时，那么 data 指针可以用于保存业务所需要的数据，例如在 HTTP 框架中就保存了 ngx_http_request_t 对象
     * 连接归化给连接池时，data 指针再次变成 next 指针，指向下一个空闲连接
     */
    void               *data;
    ngx_event_t        *read;       // 连接对应的读事件，从 read_events 数组中获取
    ngx_event_t        *write;      // 连接对应的写事件，从 write_events 数组中获取

    ngx_socket_t        fd;         // 当前连接的 TCP socket 句柄

    /*
     * 直接接收 socket 数据时调用的方法
     * 函数原型: ssize_t (*ngx_recv_pt)(ngx_connection_t *c, u_char *buf, size_t size);
     */
    ngx_recv_pt         recv;

    /*
     * 直接写入 socket 数据时调用的方法
     * 函数原型: ssize_t (*ngx_send_pt)(ngx_connection_t *c, u_char *buf, size_t size);
     */
    ngx_send_pt         send;
    ngx_recv_chain_pt   recv_chain;
    ngx_send_chain_pt   send_chain;

    ngx_listening_t    *listening;  // socket 监听对象

    off_t               sent;       // 发送偏移量，表示已经向 socket 写入了多少数据

    ngx_log_t          *log;        // 日志对象

    ngx_pool_t         *pool;       // 内存池对象，每一个连接在建立时都会创建一个 ngx_pool_t 对象

    int                 type;

    struct sockaddr    *sockaddr;   // 对端的 IP 地址信息
    socklen_t           socklen;
    ngx_str_t           addr_text;

    ngx_proxy_protocol_t  *proxy_protocol;

#if (NGX_SSL || NGX_COMPAT)
    ngx_ssl_connection_t  *ssl;
#endif

    ngx_udp_connection_t  *udp;

    struct sockaddr    *local_sockaddr;
    socklen_t           local_socklen;

    ngx_buf_t          *buffer;     // 接收缓冲区

    ngx_queue_t         queue;

    ngx_atomic_uint_t   number;

    ngx_msec_t          start_time;
    ngx_uint_t          requests;

    unsigned            buffered:8;

    unsigned            log_error:3;     /* ngx_connection_log_error_e */

    unsigned            timedout:1;
    unsigned            error:1;
    unsigned            destroyed:1;

    unsigned            idle:1;
    unsigned            reusable:1;
    unsigned            close:1;
    unsigned            shared:1;

    unsigned            sendfile:1;
    unsigned            sndlowat:1;
    unsigned            tcp_nodelay:2;   /* ngx_connection_tcp_nodelay_e */
    unsigned            tcp_nopush:2;    /* ngx_connection_tcp_nopush_e */

    unsigned            need_last_buf:1;

#if (NGX_HAVE_AIO_SENDFILE || NGX_COMPAT)
    unsigned            busy_count:2;
#endif

#if (NGX_THREADS || NGX_COMPAT)
    ngx_thread_task_t  *sendfile_task;
#endif
};


#define ngx_set_connection_log(c, l)                                         \
                                                                             \
    c->log->file = l->file;                                                  \
    c->log->next = l->next;                                                  \
    c->log->writer = l->writer;                                              \
    c->log->wdata = l->wdata;                                                \
    if (!(c->log->log_level & NGX_LOG_DEBUG_CONNECTION)) {                   \
        c->log->log_level = l->log_level;                                    \
    }


// 将 sockaddr 和 socklen 保存至 ngx_cycle_t 中的 listening 动态数组中
ngx_listening_t *ngx_create_listening(ngx_conf_t *cf, struct sockaddr *sockaddr,
    socklen_t socklen);

// 将新的 ngx_listening_t 对象 ls 逐个地赋值给 worker_processes 中的每一个 ngx_cycle_t 的 listening 数组对象
ngx_int_t ngx_clone_listening(ngx_cycle_t *cycle, ngx_listening_t *ls);

ngx_int_t ngx_set_inherited_sockets(ngx_cycle_t *cycle);


// 对监听套接字进行 socket 选项设置，例如 SO_REUSEADDR、SO_REUSEPORT 等等，然后进行 bind() 以及 listen()，端口从这个函数开始进行正式地监听
ngx_int_t ngx_open_listening_sockets(ngx_cycle_t *cycle);

void ngx_configure_listening_sockets(ngx_cycle_t *cycle);

void ngx_close_listening_sockets(ngx_cycle_t *cycle);

void ngx_close_idle_connections(ngx_cycle_t *cycle);
ngx_int_t ngx_connection_local_sockaddr(ngx_connection_t *c, ngx_str_t *s,
    ngx_uint_t port);
ngx_int_t ngx_tcp_nodelay(ngx_connection_t *c);
ngx_int_t ngx_connection_error(ngx_connection_t *c, ngx_err_t err, char *text);

ngx_connection_t *ngx_get_connection(ngx_socket_t s, ngx_log_t *log);

// 在 ngx_close_connection() 中调用了 ngx_free_connection() 函数，一个是关闭连接，一个是将连接从连接池中移入至空闲链表中
void ngx_close_connection(ngx_connection_t *c);
void ngx_free_connection(ngx_connection_t *c);

void ngx_reusable_connection(ngx_connection_t *c, ngx_uint_t reusable);

#endif /* _NGX_CONNECTION_H_INCLUDED_ */
