
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_H_INCLUDED_
#define _NGX_EVENT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#define NGX_INVALID_INDEX  0xd0d0d0d0


#if (NGX_HAVE_IOCP)

typedef struct {
    WSAOVERLAPPED    ovlp;
    ngx_event_t     *event;
    int              error;
} ngx_event_ovlp_t;

#endif

/*
 * nginx 事件表示，可以通俗的认为 ngx_event_t 对应着 epoll 的 epoll_event
 * 最核心的部分其实是 ngx_event_handler_pt  handler，也就是回调函数，表示当有事件发生了，该如何进行处理
 */
struct ngx_event_s {

    /*
     *  与该事件相关的对象，通常会指向 ngx_connection_t 对象，表示一个 socket 连接。
     *  在开启了异步文件 I/O 时，data 将会指向 ngx_event_aio_t 结构体
     */
    void            *data;

    // 可写标志位，值为 1 时表示可写。对于 TCP 连接来说，就是发送缓冲区未满
    unsigned         write:1;

    // 是否有新的连接建立，值为 1 时表示有新的连接建立，对于 TCP 连接来说，其实就是 backlog 队列中存在已完成握手的连接
    unsigned         accept:1;

    /* used to detect the stale events in kqueue and epoll
     * 在 kqueue 和 epoll 中使用，用于区分当前事件是否过期，提供给事件驱动模块，我们可以在 ngx_epoll_module 中看到其使用方法
     */
    unsigned         instance:1;

    /*
     * the event was passed or would be passed to a kernel;
     * in aio mode - operation was posted.
     * 表示当前事件是否活跃
     */
    unsigned         active:1;

    unsigned         disabled:1;

    /* the ready event; in aio mode 0 means that no operation can be posted
     * 事件是否就绪标志位，在 HTTP 框架中会检查事件的 ready 标志位是否为 1，以确定是否可以发送或接收响应
     */
    unsigned         ready:1;

    // epoll 无意义
    unsigned         oneshot:1;

    /* aio operation is complete
     * 表示 AIO 是否完成
     */
    unsigned         complete:1;

    // 结束标志位，值为 1 时表示当前处理的流已经结束
    unsigned         eof:1;

    // 错误标志位，为 1 时表示处理过程中出现错误
    unsigned         error:1;

    // 超时标志位
    unsigned         timedout:1;

    // 表示当前事件是否存在于超时定时器中
    unsigned         timer_set:1;

    unsigned         delayed:1;

    // 是否延迟建立 TCP 连接，当值为 1 时，TCP 连接并不在三次握手时建立，而是等到客户端发送数据时才建立
    unsigned         deferred_accept:1;

    /* the pending eof reported by kqueue, epoll or in aio chain operation */
    unsigned         pending_eof:1;

    unsigned         posted:1;

    unsigned         closed:1;

    /* to test on worker exit */
    unsigned         channel:1;
    unsigned         resolver:1;

    unsigned         cancelable:1;

#if (NGX_HAVE_KQUEUE)
    unsigned         kq_vnode:1;

    /* the pending errno reported by kqueue */
    int              kq_errno;
#endif

    /*
     * kqueue only:
     *   accept:     number of sockets that wait to be accepted
     *   read:       bytes to read when event is ready
     *               or lowat when event is set with NGX_LOWAT_EVENT flag
     *   write:      available space in buffer when event is ready
     *               or lowat when event is set with NGX_LOWAT_EVENT flag
     *
     * iocp: TODO
     *
     * otherwise:
     *   accept:     1 if accept many, 0 otherwise
     *   read:       bytes to read when event is ready, -1 if not known
     */

    int              available;

    // 事件发生时的回调函数，由事件消费模块重新实现
    ngx_event_handler_pt  handler;


#if (NGX_HAVE_IOCP)
    ngx_event_ovlp_t ovlp;
#endif

    ngx_uint_t       index;

    // 日志对象
    ngx_log_t       *log;

    // 定时器节点
    ngx_rbtree_node_t   timer;

    /* the posted queue */
    // post 事件双向链表指针
    ngx_queue_t      queue;

#if 0

    /* the threads support */

    /*
     * the event thread context, we store it here
     * if $(CC) does not understand __thread declaration
     * and pthread_getspecific() is too costly
     */

    void            *thr_ctx;

#if (NGX_EVENT_T_PADDING)

    /* event should not cross cache line in SMP */

    uint32_t         padding[NGX_EVENT_T_PADDING];
#endif
#endif
};


#if (NGX_HAVE_FILE_AIO)

struct ngx_event_aio_s {
    void                      *data;
    ngx_event_handler_pt       handler;
    ngx_file_t                *file;

    ngx_fd_t                   fd;

#if (NGX_HAVE_AIO_SENDFILE || NGX_COMPAT)
    ssize_t                  (*preload_handler)(ngx_buf_t *file);
#endif

#if (NGX_HAVE_EVENTFD)
    int64_t                    res;
#endif

#if !(NGX_HAVE_EVENTFD) || (NGX_TEST_BUILD_EPOLL)
    ngx_err_t                  err;
    size_t                     nbytes;
#endif

    ngx_aiocb_t                aiocb;
    ngx_event_t                event;
};

#endif

/*
 * ngx_event_actions_t 表示一个 interface，其中最为重要的方法就是 add、del 以及 process_events，
 * 这 3 个函数是 nginx 实现事件驱动的核心
 */
typedef struct {

    /*
     * 将感兴趣的事件（如 socket 可读/可写等）添加至 OS 提供的事件驱动机制中，以 epoll 为例，add 方法将会调用
     * epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) 将 event 保存至 epoll 的红黑树中，op 参数为 EPOLL_CTL_ADD。
     * 后续我们可以通过下面的 process_events 来获取已添加的事件
     */
    ngx_int_t  (*add)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);

    // 删除事件的方法，后续无法通过 process_events 获取该事件
    ngx_int_t  (*del)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);

    // enable 和 disable 这两个方法似乎并没有用到，可能是出于兼容性的考虑而设置的这两个方法，因为 enable/disable 对应着 add/del
    ngx_int_t  (*enable)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);
    ngx_int_t  (*disable)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);

    // 下面两个方法分别表示向事件驱动中添加和删除一个连接
    ngx_int_t  (*add_conn)(ngx_connection_t *c);
    ngx_int_t  (*del_conn)(ngx_connection_t *c, ngx_uint_t flags);

    ngx_int_t  (*notify)(ngx_event_handler_pt handler);

    // 处理和分发事件的核心，仅会在 ngx_process_events_and_timers 中被调用
    ngx_int_t  (*process_events)(ngx_cycle_t *cycle, ngx_msec_t timer,
                                 ngx_uint_t flags);

    // 下面两个方法分别表示在初始化事件驱动以及退出事件驱动时调用的方法，相当于一个"切口"函数
    ngx_int_t  (*init)(ngx_cycle_t *cycle, ngx_msec_t timer);
    void       (*done)(ngx_cycle_t *cycle);
} ngx_event_actions_t;


extern ngx_event_actions_t   ngx_event_actions;
#if (NGX_HAVE_EPOLLRDHUP)
extern ngx_uint_t            ngx_use_epoll_rdhup;
#endif


/*
 * The event filter requires to read/write the whole data:
 * select, poll, /dev/poll, kqueue, epoll.
 */
#define NGX_USE_LEVEL_EVENT      0x00000001

/*
 * The event filter is deleted after a notification without an additional
 * syscall: kqueue, epoll.
 */
#define NGX_USE_ONESHOT_EVENT    0x00000002

/*
 * The event filter notifies only the changes and an initial level:
 * kqueue, epoll.
 */
#define NGX_USE_CLEAR_EVENT      0x00000004

/*
 * The event filter has kqueue features: the eof flag, errno,
 * available data, etc.
 */
#define NGX_USE_KQUEUE_EVENT     0x00000008

/*
 * The event filter supports low water mark: kqueue's NOTE_LOWAT.
 * kqueue in FreeBSD 4.1-4.2 has no NOTE_LOWAT so we need a separate flag.
 */
#define NGX_USE_LOWAT_EVENT      0x00000010

/*
 * The event filter requires to do i/o operation until EAGAIN: epoll.
 */
#define NGX_USE_GREEDY_EVENT     0x00000020

/*
 * The event filter is epoll.
 */
#define NGX_USE_EPOLL_EVENT      0x00000040

/*
 * Obsolete.
 */
#define NGX_USE_RTSIG_EVENT      0x00000080

/*
 * Obsolete.
 */
#define NGX_USE_AIO_EVENT        0x00000100

/*
 * Need to add socket or handle only once: i/o completion port.
 */
#define NGX_USE_IOCP_EVENT       0x00000200

/*
 * The event filter has no opaque data and requires file descriptors table:
 * poll, /dev/poll.
 */
#define NGX_USE_FD_EVENT         0x00000400

/*
 * The event module handles periodic or absolute timer event by itself:
 * kqueue in FreeBSD 4.4, NetBSD 2.0, and MacOSX 10.4, Solaris 10's event ports.
 */
#define NGX_USE_TIMER_EVENT      0x00000800

/*
 * All event filters on file descriptor are deleted after a notification:
 * Solaris 10's event ports.
 */
#define NGX_USE_EVENTPORT_EVENT  0x00001000

/*
 * The event filter support vnode notifications: kqueue.
 */
#define NGX_USE_VNODE_EVENT      0x00002000


/*
 * The event filter is deleted just before the closing file.
 * Has no meaning for select and poll.
 * kqueue, epoll, eventport:         allows to avoid explicit delete,
 *                                   because filter automatically is deleted
 *                                   on file close,
 *
 * /dev/poll:                        we need to flush POLLREMOVE event
 *                                   before closing file.
 */
#define NGX_CLOSE_EVENT    1

/*
 * disable temporarily event filter, this may avoid locks
 * in kernel malloc()/free(): kqueue.
 */
#define NGX_DISABLE_EVENT  2

/*
 * event must be passed to kernel right now, do not wait until batch processing.
 */
#define NGX_FLUSH_EVENT    4


/* these flags have a meaning only for kqueue */
#define NGX_LOWAT_EVENT    0
#define NGX_VNODE_EVENT    0


#if (NGX_HAVE_EPOLL) && !(NGX_HAVE_EPOLLRDHUP)
#define EPOLLRDHUP         0
#endif


#if (NGX_HAVE_KQUEUE)

#define NGX_READ_EVENT     EVFILT_READ
#define NGX_WRITE_EVENT    EVFILT_WRITE

#undef  NGX_VNODE_EVENT
#define NGX_VNODE_EVENT    EVFILT_VNODE

/*
 * NGX_CLOSE_EVENT, NGX_LOWAT_EVENT, and NGX_FLUSH_EVENT are the module flags
 * and they must not go into a kernel so we need to choose the value
 * that must not interfere with any existent and future kqueue flags.
 * kqueue has such values - EV_FLAG1, EV_EOF, and EV_ERROR:
 * they are reserved and cleared on a kernel entrance.
 */
#undef  NGX_CLOSE_EVENT
#define NGX_CLOSE_EVENT    EV_EOF

#undef  NGX_LOWAT_EVENT
#define NGX_LOWAT_EVENT    EV_FLAG1

#undef  NGX_FLUSH_EVENT
#define NGX_FLUSH_EVENT    EV_ERROR

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  EV_ONESHOT
#define NGX_CLEAR_EVENT    EV_CLEAR

#undef  NGX_DISABLE_EVENT
#define NGX_DISABLE_EVENT  EV_DISABLE


#elif (NGX_HAVE_DEVPOLL && !(NGX_TEST_BUILD_DEVPOLL)) \
      || (NGX_HAVE_EVENTPORT && !(NGX_TEST_BUILD_EVENTPORT))

#define NGX_READ_EVENT     POLLIN
#define NGX_WRITE_EVENT    POLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1


#elif (NGX_HAVE_EPOLL) && !(NGX_TEST_BUILD_EPOLL)

#define NGX_READ_EVENT     (EPOLLIN|EPOLLRDHUP)
#define NGX_WRITE_EVENT    EPOLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_CLEAR_EVENT    EPOLLET
#define NGX_ONESHOT_EVENT  0x70000000
#if 0
#define NGX_ONESHOT_EVENT  EPOLLONESHOT
#endif

#if (NGX_HAVE_EPOLLEXCLUSIVE)
#define NGX_EXCLUSIVE_EVENT  EPOLLEXCLUSIVE
#endif

#elif (NGX_HAVE_POLL)

#define NGX_READ_EVENT     POLLIN
#define NGX_WRITE_EVENT    POLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1


#else /* select */

#define NGX_READ_EVENT     0
#define NGX_WRITE_EVENT    1

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1

#endif /* NGX_HAVE_KQUEUE */


#if (NGX_HAVE_IOCP)
#define NGX_IOCP_ACCEPT      0
#define NGX_IOCP_IO          1
#define NGX_IOCP_CONNECT     2
#endif


#if (NGX_TEST_BUILD_EPOLL)
#define NGX_EXCLUSIVE_EVENT  0
#endif


#ifndef NGX_CLEAR_EVENT
#define NGX_CLEAR_EVENT    0    /* dummy declaration */
#endif


#define ngx_process_events   ngx_event_actions.process_events
#define ngx_done_events      ngx_event_actions.done

#define ngx_add_event        ngx_event_actions.add
#define ngx_del_event        ngx_event_actions.del
#define ngx_add_conn         ngx_event_actions.add_conn
#define ngx_del_conn         ngx_event_actions.del_conn

#define ngx_notify           ngx_event_actions.notify

#define ngx_add_timer        ngx_event_add_timer
#define ngx_del_timer        ngx_event_del_timer


extern ngx_os_io_t  ngx_io;

#define ngx_recv             ngx_io.recv
#define ngx_recv_chain       ngx_io.recv_chain
#define ngx_udp_recv         ngx_io.udp_recv
#define ngx_send             ngx_io.send
#define ngx_send_chain       ngx_io.send_chain
#define ngx_udp_send         ngx_io.udp_send
#define ngx_udp_send_chain   ngx_io.udp_send_chain


#define NGX_EVENT_MODULE      0x544E5645  /* "EVNT" */
#define NGX_EVENT_CONF        0x02000000


typedef struct {
    ngx_uint_t    connections;
    ngx_uint_t    use;

    ngx_flag_t    multi_accept;
    ngx_flag_t    accept_mutex;

    ngx_msec_t    accept_mutex_delay;

    u_char       *name;

#if (NGX_DEBUG)
    ngx_array_t   debug_connection;
#endif
} ngx_event_conf_t;


/*
 * nginx 依赖于事件驱动机制，或者说 nginx 就是一个事件驱动 Web Server。
 * 因此，nginx 非常依赖于操作系统所提供的事件驱动机制。比如 Linux 2.6 版本之前的 poll，2.6 版本之后的 epoll；
 * 而在 FreeBSD 上则可以使用 kqueue，Solaris 10上可以使用 eventport 等。
 *
 * 在 src/event/modules 中我们就可以看到 ngx_poll_module.c、ngx_select_module.c、ngx_epoll_module.c、ngx_kqueue_module.c
 * 这些具体的实现文件，也就是说，根据操作系统的不同以及配置文件的不同，nginx 将选择不同的内核事件驱动机制。
 *
 * 下面是一个比较通用的 nginx events 块的配置信息，当我们配置了 use epoll 时，nginx 将会选取 epoll 作为事件驱动机制的底层实现，
 * 这个过程是由 ngx_event_core_module 在初始化时完成的。
 *
 * events {
 *      accept_mutex on;             # 设置网路连接序列化，防止惊群现象发生，默认为on
 *      multi_accept on;             # 设置一个进程是否同时接受多个网络连接，默认为off
 *      use epoll;                   # 事件驱动模型，select|poll|kqueue|epoll|eventport
 *      worker_connections  1024;    # 最大连接数，默认为512
 * }
 *
 * 如果我们没有明确的表示到底使用哪种事件驱动模型，那么 nginx 将会根据平台和版本进行选择。
 *
 * ngx_event_module_t 其实就是对这些事件驱动机制的一个封装，或者是提供了一个 Interface
 */
typedef struct {
    ngx_str_t              *name;       // 事件模块的名称

    // 两个回调函数指针
    void                 *(*create_conf)(ngx_cycle_t *cycle);
    char                 *(*init_conf)(ngx_cycle_t *cycle, void *conf);

    // actions 其实就是一个 interface，ngx_event_actions_t 中定义了 10 个与事件驱动机制相关的方法，
    // ngx_xxx_module.c 需要实现这些方法，从而对上层提供统一的调用接口。
    ngx_event_actions_t     actions;
} ngx_event_module_t;


extern ngx_atomic_t          *ngx_connection_counter;

extern ngx_atomic_t          *ngx_accept_mutex_ptr;
extern ngx_shmtx_t            ngx_accept_mutex;
extern ngx_uint_t             ngx_use_accept_mutex;
extern ngx_uint_t             ngx_accept_events;
extern ngx_uint_t             ngx_accept_mutex_held;
extern ngx_msec_t             ngx_accept_mutex_delay;
extern ngx_int_t              ngx_accept_disabled;


#if (NGX_STAT_STUB)

extern ngx_atomic_t  *ngx_stat_accepted;
extern ngx_atomic_t  *ngx_stat_handled;
extern ngx_atomic_t  *ngx_stat_requests;
extern ngx_atomic_t  *ngx_stat_active;
extern ngx_atomic_t  *ngx_stat_reading;
extern ngx_atomic_t  *ngx_stat_writing;
extern ngx_atomic_t  *ngx_stat_waiting;

#endif


#define NGX_UPDATE_TIME         1
#define NGX_POST_EVENTS         2


extern sig_atomic_t           ngx_event_timer_alarm;
extern ngx_uint_t             ngx_event_flags;
extern ngx_module_t           ngx_events_module;
extern ngx_module_t           ngx_event_core_module;


#define ngx_event_get_conf(conf_ctx, module)                                  \
             (*(ngx_get_conf(conf_ctx, ngx_events_module))) [module.ctx_index]



void ngx_event_accept(ngx_event_t *ev);
#if !(NGX_WIN32)
void ngx_event_recvmsg(ngx_event_t *ev);
void ngx_udp_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
#endif
void ngx_delete_udp_connection(void *data);
ngx_int_t ngx_trylock_accept_mutex(ngx_cycle_t *cycle);
ngx_int_t ngx_enable_accept_events(ngx_cycle_t *cycle);
u_char *ngx_accept_log_error(ngx_log_t *log, u_char *buf, size_t len);
#if (NGX_DEBUG)
void ngx_debug_accepted_connection(ngx_event_conf_t *ecf, ngx_connection_t *c);
#endif


void ngx_process_events_and_timers(ngx_cycle_t *cycle);
ngx_int_t ngx_handle_read_event(ngx_event_t *rev, ngx_uint_t flags);
ngx_int_t ngx_handle_write_event(ngx_event_t *wev, size_t lowat);


#if (NGX_WIN32)
void ngx_event_acceptex(ngx_event_t *ev);
ngx_int_t ngx_event_post_acceptex(ngx_listening_t *ls, ngx_uint_t n);
u_char *ngx_acceptex_log_error(ngx_log_t *log, u_char *buf, size_t len);
#endif


ngx_int_t ngx_send_lowat(ngx_connection_t *c, size_t lowat);


/* used in ngx_log_debugX() */
#define ngx_event_ident(p)  ((ngx_connection_t *) (p))->fd


#include <ngx_event_timer.h>
#include <ngx_event_posted.h>

#if (NGX_WIN32)
#include <ngx_iocp_module.h>
#endif


#endif /* _NGX_EVENT_H_INCLUDED_ */
