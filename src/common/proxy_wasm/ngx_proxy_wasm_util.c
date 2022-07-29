#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_event.h>
#include <ngx_wavm.h>
#include <ngx_proxy_wasm.h>


#define NGX_PROXY_WASM_PTR_SIZE  4


static ngx_str_t  ngx_proxy_wasm_errlist[] = {
    ngx_null_string,
    ngx_string("unknown ABI version"),
    ngx_string("incompatible ABI version"),
    ngx_string("incompatible host interface"),
    ngx_string("incompatible SDK interface"),
    ngx_string("instantiation failed"),
    ngx_string("instance trapped"),
    ngx_string("initialization failed"),
    ngx_string("dispatch failed"),
    ngx_string("not yieldable"),
    ngx_string("unknown error")
};


static ngx_inline ngx_str_t *
ngx_proxy_wasm_filter_strerror(ngx_proxy_wasm_err_e err)
{
    ngx_str_t  *msg;

    msg = ((ngx_uint_t) err < NGX_PROXY_WASM_ERR_UNKNOWN)
              ? &ngx_proxy_wasm_errlist[err]
              : &ngx_proxy_wasm_errlist[NGX_PROXY_WASM_ERR_UNKNOWN];

    return msg;
}


void
ngx_proxy_wasm_log_error(ngx_uint_t level, ngx_log_t *log,
    ngx_proxy_wasm_err_e err, const char *fmt, ...)
{
    va_list     args;
    u_char     *p, *last, buf[NGX_MAX_ERROR_STR];
    ngx_str_t  *errmsg = NULL;

    last = buf + NGX_MAX_ERROR_STR;
    p = &buf[0];

    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

    if (err) {
        errmsg = ngx_proxy_wasm_filter_strerror(err);
        p = ngx_slprintf(p, last, " (%V)", errmsg);
    }

    ngx_wasm_log_error(level, log, 0, "%*s", p - buf, buf);
}


ngx_uint_t
ngx_proxy_wasm_pairs_count(ngx_list_t *list)
{
    size_t            i, c = 0;
    ngx_list_part_t  *part;
    ngx_table_elt_t  *elt;

    part = &list->part;
    elt = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

        if (elt[i].hash == 0) {
            continue;
        }

        c++;
    }

    return c;
}


size_t
ngx_proxy_wasm_pairs_size(ngx_list_t *list, ngx_array_t *extras, ngx_uint_t max)
{
    size_t            i, n, size;
    ngx_list_part_t  *part;
    ngx_table_elt_t  *elt;

    part = &list->part;
    elt = part->elts;

    size = NGX_PROXY_WASM_PTR_SIZE; /* headers count */

    for (i = 0, n = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

        if (elt[i].hash == 0) {
            continue;
        }

        size += NGX_PROXY_WASM_PTR_SIZE * 2;
        size += elt[i].key.len + 1;
        size += elt[i].value.len + 1;

#if 0
        dd("key: %.*s, value: %.*s, size: %lu",
           (int) elt[i].key.len, elt[i].key.data,
           (int) elt[i].value.len, elt[i].value.data, size);
#endif

        n++;

        if (max && n >= max) {
            break;
        }
    }

    if (extras) {
        elt = extras->elts;

        for (i = 0; i < extras->nelts; i++, n++) {
            size += NGX_PROXY_WASM_PTR_SIZE * 2;
            size += elt[i].key.len + 1;
            size += elt[i].value.len + 1;

#if 0
            dd("extra key: %.*s, extra value: %.*s, size: %lu",
               (int) elt[i].key.len, elt[i].key.data,
               (int) elt[i].value.len, elt[i].value.data, size);
#endif

            if (max && n >= max) {
                break;
            }
        }
    }

    return size;
}


void
ngx_proxy_wasm_pairs_marshal(ngx_list_t *list, ngx_array_t *extras, u_char *buf,
    ngx_uint_t max, ngx_uint_t *truncated)
{
    size_t            i, n = 0;
    uint32_t          count;
    ngx_table_elt_t  *elt;
    ngx_list_part_t  *part;

    count = ngx_proxy_wasm_pairs_count(list);

    if (extras) {
        count += extras->nelts;
    }

    if (max && count > max) {
        count = max;

        if (truncated) {
            *truncated = count;
        }
    }

    *((uint32_t *) buf) = count;
    buf += NGX_PROXY_WASM_PTR_SIZE;

    if (extras) {
        elt = extras->elts;

        for (i = 0; i < extras->nelts && n < count; i++, n++) {
            *((uint32_t *) buf) = elt[i].key.len;
            buf += NGX_PROXY_WASM_PTR_SIZE;
            *((uint32_t *) buf) = elt[i].value.len;
            buf += NGX_PROXY_WASM_PTR_SIZE;
        }
    }

    part = &list->part;
    elt = part->elts;

    for (i = 0; n < count; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

       if (elt[i].hash == 0) {
           continue;
       }

        *((uint32_t *) buf) = elt[i].key.len;
        buf += NGX_PROXY_WASM_PTR_SIZE;
        *((uint32_t *) buf) = elt[i].value.len;
        buf += NGX_PROXY_WASM_PTR_SIZE;

        n++;
    }

    ngx_wasm_assert(n == count);

    n = 0;

    if (extras) {
        elt = extras->elts;

        for (i = 0; i < extras->nelts && n < count; i++, n++) {
            buf = ngx_cpymem(buf, elt[i].key.data, elt[i].key.len);
            *buf++ = '\0';
            buf = ngx_cpymem(buf, elt[i].value.data, elt[i].value.len);
            *buf++ = '\0';
        }
    }

    part = &list->part;
    elt = part->elts;

    for (i = 0; n < count; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elt = part->elts;
            i = 0;
        }

        if (elt[i].hash == 0) {
            continue;
        }

        buf = ngx_cpymem(buf, elt[i].key.data, elt[i].key.len);
        *buf++ = '\0';
        buf = ngx_cpymem(buf, elt[i].value.data, elt[i].value.len);
        *buf++ = '\0';

        n++;
    }

    ngx_wasm_assert(n == count);
}


ngx_int_t
ngx_proxy_wasm_pairs_unmarshal(ngx_array_t *dst, ngx_pool_t *pool,
    ngx_proxy_wasm_marshalled_map_t *map)
{
    size_t            i;
    uint32_t          count = 0;
    u_char           *buf;
    ngx_table_elt_t  *elt;

    buf = map->data;

    if (map->len) {
        count = *((uint32_t *) buf);
        buf += NGX_PROXY_WASM_PTR_SIZE;
    }

    if (ngx_array_init(dst, pool, count, sizeof(ngx_table_elt_t)) != NGX_OK) {
        return NGX_ERROR;
    }

    for (i = 0; i < count; i++) {
        elt = ngx_array_push(dst);
        if (elt == NULL) {
            goto failed;
        }

        elt->hash = 0;
        elt->lowcase_key = NULL;

        elt->key.len = *((uint32_t *) buf);
        buf += NGX_PROXY_WASM_PTR_SIZE;
        elt->value.len = *((uint32_t *) buf);
        buf += NGX_PROXY_WASM_PTR_SIZE;
    }

    for (i = 0; i < dst->nelts; i++) {
        elt = &((ngx_table_elt_t *) dst->elts)[i];

        elt->key.data = ngx_pnalloc(pool, elt->key.len + 1);
        if (elt->key.data == NULL) {
            goto failed;
        }

        ngx_memcpy(elt->key.data, buf, elt->key.len + 1);
        buf += elt->key.len + 1;

        elt->value.data = ngx_pnalloc(pool, elt->value.len + 1);
        if (elt->value.data == NULL) {
            goto failed;
        }

        ngx_memcpy(elt->value.data, buf, elt->value.len + 1);
        buf += elt->value.len + 1;

#if 0
        dd("key: %.*s, value: %.*s",
           (int) elt->key.len, elt->key.data,
           (int) elt->value.len, elt->value.data);
#endif
    }

    return NGX_OK;

failed:

    ngx_array_destroy(dst);

    return NGX_ERROR;
}


void
ngx_proxy_wasm_filter_tick_handler(ngx_event_t *ev)
{
    ngx_int_t                       rc;
    ngx_log_t                      *log = ev->log;
    ngx_proxy_wasm_filter_ctx_t    *fctx = ev->data;
    ngx_proxy_wasm_filter_t        *filter = fctx->filter;
    ngx_proxy_wasm_instance_ctx_t  *ictx;

    ngx_wasm_assert(fctx->root_id == NGX_PROXY_WASM_ROOT_CTX_ID);

    ngx_free(ev);

    fctx->ev = NULL;

    if (ngx_exiting || !filter->proxy_on_timer_ready) {
        return;
    }

    ictx = filter->root_ictx;
    if (ictx == NULL) {
        ngx_wasm_log_error(NGX_LOG_ERR, log, 0,
                           "tick_handler: no root instance");
        return;
    }

    ngx_wavm_instance_set_data(ictx->instance, ictx, log);

    rc = ngx_proxy_wasm_resume(ictx, filter, fctx,
                               NGX_PROXY_WASM_STEP_ON_TIMER, NULL);
    if (rc != NGX_OK) {
#if 0
        if (fctx->ecode == NGX_PROXY_WASM_ERR_INSTANCE_TRAPPED) {
            filter->root_ictx = ngx_proxy_wasm_instance_get(filter,
                                                            ictx->store, ictx->log);
            if (filter->root_ictx == NULL) {
                goto nomem;
            }

            ngx_proxy_wasm_instance_release(ictx, 0);
        }
#endif
        return;
    }

    if (!ngx_exiting) {
        fctx->ev = ngx_calloc(sizeof(ngx_event_t), log);
        if (fctx->ev == NULL) {
            goto nomem;
        }

        fctx->ev->handler = ngx_proxy_wasm_filter_tick_handler;
        fctx->ev->data = fctx;
        fctx->ev->log = log;

        ngx_add_timer(fctx->ev, filter->tick_period);
    }

    return;

nomem:

    ngx_wasm_log_error(NGX_LOG_CRIT, log, 0,
                       "tick_handler: no memory");
}
