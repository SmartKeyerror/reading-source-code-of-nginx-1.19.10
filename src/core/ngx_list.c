
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


// 创建一个空的单向链表对象，需要注意的是，ngx_list_part_t 也被初始化了，也就是说
// 在不创建新的 ngx_list_part_t 的情况下，ngx_list_t 此时最多能够容纳 n 个元素
ngx_list_t *
ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    ngx_list_t  *list;

    list = ngx_palloc(pool, sizeof(ngx_list_t));
    if (list == NULL) {
        return NULL;
    }

    if (ngx_list_init(list, pool, n, size) != NGX_OK) {
        return NULL;
    }

    return list;
}


void *
ngx_list_push(ngx_list_t *l)
{
    void             *elt;
    ngx_list_part_t  *last;

    // 获取单向链表的 tail 节点
    last = l->last;

    // 如果最后一个节点的数组已经写满了，那么此时就需要再创建一个 ngx_list_part_t 对象
    if (last->nelts == l->nalloc) {
        // 从 pool 内存池中分配出一个 ngx_list_part_t 对象
        last = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
        if (last == NULL) {
            return NULL;
        }

        // 从 pool 内存池中分配出 elts 数组所需内存
        last->elts = ngx_palloc(l->pool, l->nalloc * l->size);
        if (last->elts == NULL) {
            return NULL;
        }

        last->nelts = 0;
        last->next = NULL;

        l->last->next = last;
        l->last = last;
    }

    // 取出数组空闲位置的内存地址，返回给调用方
    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;

    return elt;
}
