
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

// 与定时器事件相关内容

#ifndef _NGX_TIMES_H_INCLUDED_
#define _NGX_TIMES_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/*
 *  nginx 在获取时间时处于性能的考虑，并不会每次都调用 gettimeofday() 方法，而是将时间缓存在内存中。
 *
 */

typedef struct {
    // 1970 年 1 月 1 日凌晨到当前的时间秒数
    time_t      sec;
    // sec 只能精确到秒，所以使用 msec 来表示毫秒数
    ngx_uint_t  msec;
    // 时区
    ngx_int_t   gmtoff;
} ngx_time_t;


void ngx_time_init(void);
void ngx_time_update(void);
void ngx_time_sigsafe_update(void);
u_char *ngx_http_time(u_char *buf, time_t t);
u_char *ngx_http_cookie_time(u_char *buf, time_t t);
void ngx_gmtime(time_t t, ngx_tm_t *tp);

time_t ngx_next_time(time_t when);
#define ngx_next_time_n      "mktime()"


extern volatile ngx_time_t  *ngx_cached_time;

#define ngx_time()           ngx_cached_time->sec
#define ngx_timeofday()      (ngx_time_t *) ngx_cached_time

extern volatile ngx_str_t    ngx_cached_err_log_time;
extern volatile ngx_str_t    ngx_cached_http_time;
extern volatile ngx_str_t    ngx_cached_http_log_time;
extern volatile ngx_str_t    ngx_cached_http_log_iso8601;
extern volatile ngx_str_t    ngx_cached_syslog_time;

/*
 * milliseconds elapsed since some unspecified point in the past
 * and truncated to ngx_msec_t, used in event timers
 */
extern volatile ngx_msec_t  ngx_current_msec;


#endif /* _NGX_TIMES_H_INCLUDED_ */
