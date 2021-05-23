
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>

/*
 * 创建 Daemon 进程，流程比较统一，可参考 https://smartkeyerror.oss-cn-shenzhen.aliyuncs.com/Phyduck/linux-network/DAEMON.pdf
 * 调用 fork() 创建子进程，而后父进程退出，此时子进程将成为孤儿进程，由 init 进程进行接管，那么其声明周期就和 OS 的声明周期相同了。
 * 紧接着调用 setsid() 创建一个新的 session 会话，和当前的 session 脱离关系，否则如果关闭了当前 session 的话，子进程将会收到 SIGHUP 信号。
 * 但是前面我们已经提到过了，SIGHUP 信号在 nginx 的运行中有着特殊的含义，即优雅地重启。再然后就是使用 umask()清除进程 的 umask，并将 STDIN
 * STDOUT 和 STDERR 进程重定向，守护进程如果不对其进行重定向的话，那么调用 printf() 等函数是没有任何意义的。
 *
 */
ngx_int_t
ngx_daemon(ngx_log_t *log)
{
    int  fd;

    switch (fork()) {
    case -1:
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "fork() failed");
        return NGX_ERROR;

    // 子进程跳出 switch 语句，继续向下执行
    case 0:
        break;

    // 父进程则直接退出
    default:
        exit(0);
    }

    ngx_parent = ngx_pid;
    ngx_pid = ngx_getpid();

    // 新建 session
    if (setsid() == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "setsid() failed");
        return NGX_ERROR;
    }

    umask(0);

    // 打开 /dev/null 文件，此时 fd 应该要大于 STDERR_FILENO，也就是大于 2
    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "open(\"/dev/null\") failed");
        return NGX_ERROR;
    }

    // 重定向 STDIN 文件描述符，dup2 的作用其实就是让 STDIN_FILENO 指向 fd 所指向的设备文件
    if (dup2(fd, STDIN_FILENO) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "dup2(STDIN) failed");
        return NGX_ERROR;
    }

    // 重定向 STDOUT 文件描述符
    if (dup2(fd, STDOUT_FILENO) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "dup2(STDOUT) failed");
        return NGX_ERROR;
    }

#if 0
    if (dup2(fd, STDERR_FILENO) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "dup2(STDERR) failed");
        return NGX_ERROR;
    }
#endif

    if (fd > STDERR_FILENO) {
        if (close(fd) == -1) {
            ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "close() failed");
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}
