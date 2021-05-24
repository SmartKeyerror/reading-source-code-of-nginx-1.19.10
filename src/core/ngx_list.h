
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_LIST_H_INCLUDED_
#define _NGX_LIST_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_list_part_s  ngx_list_part_t;

struct ngx_list_part_s {
    void             *elts;     // 即 elements，指向数组的起始地址
    ngx_uint_t        nelts;    // 即 number of elements，当前数组已经保存了多少元素，nelts 必须小于 ngx_list_t 中的 nalloc
    ngx_list_part_t  *next;     // 单向链表指针，指向下一个 ngx_list_part_t 结构
};


typedef struct {
    ngx_list_part_t  *last;     // 指向单向链表的最后一个元素，相当于一个 tail 指针
    ngx_list_part_t   part;     // 这里就很有意思了，它不是一个指针，保存的是单向链表的首个元素
    size_t            size;     // 这里指的是保存在 elts 数组中的元素大小
    ngx_uint_t        nalloc;   // 表示 elts 数组的初始大小，或者说初始容量，一旦确定便不可更改
    ngx_pool_t       *pool;     // 内存池对象，用于分配用户所需要保存对象的内存
} ngx_list_t;


ngx_list_t *ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size);

static ngx_inline ngx_int_t
ngx_list_init(ngx_list_t *list, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    list->part.elts = ngx_palloc(pool, n * size);
    if (list->part.elts == NULL) {
        return NGX_ERROR;
    }

    list->part.nelts = 0;
    list->part.next = NULL;
    list->last = &list->part;
    list->size = size;
    list->nalloc = n;
    list->pool = pool;

    return NGX_OK;
}


/*
 *
 *  the iteration through the list:
 *
 *  part = &list.part;
 *  data = part->elts;
 *
 *  for (i = 0 ;; i++) {
 *
 *      if (i >= part->nelts) {
 *          if (part->next == NULL) {
 *              break;
 *          }
 *
 *          part = part->next;
 *          data = part->elts;
 *          i = 0;
 *      }
 *
 *      ...  data[i] ...
 *
 *  }
 */


void *ngx_list_push(ngx_list_t *list);


#endif /* _NGX_LIST_H_INCLUDED_ */
