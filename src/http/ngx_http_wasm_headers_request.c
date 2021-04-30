#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_http_wasm.h>


static ngx_int_t ngx_http_wasm_set_builtin_multi_header_handler(
    ngx_http_request_t *r, ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_wasm_set_host_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_wasm_set_connection_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_wasm_set_ua_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_wasm_set_cl_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_clear_builtin_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value);


static ngx_http_wasm_header_t  ngx_http_wasm_special_req_headers[] = {

    { ngx_string("Host"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, host),
                 ngx_http_wasm_set_host_header_handler },

    { ngx_string("Connection"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, connection),
                 ngx_http_wasm_set_connection_header_handler },

    { ngx_string("If-Modified-Since"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, if_modified_since),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("If-Unmodified-Since"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, if_unmodified_since),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("If-Match"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, if_match),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("If-None-Match"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, if_none_match),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("User-Agent"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, user_agent),
                 ngx_http_wasm_set_ua_header_handler },

    { ngx_string("Referer"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, referer),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Content-Length"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, content_length),
                 ngx_http_wasm_set_cl_header_handler },

    { ngx_string("Content-Type"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, content_type),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Range"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, range),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("If-Range"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, if_range),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Transfer-Encoding"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, transfer_encoding),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Expect"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, expect),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Upgrade"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, upgrade),
                 ngx_http_wasm_set_builtin_header_handler },

#if (NGX_HTTP_GZIP)
    { ngx_string("Accept-Encoding"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, accept_encoding),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Via"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, via),
                 ngx_http_wasm_set_builtin_header_handler },
#endif

    { ngx_string("Authorization"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, authorization),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Keep-Alive"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, keep_alive),
                 ngx_http_wasm_set_builtin_header_handler },

#if (NGX_HTTP_X_FORWARDED_FOR)
    { ngx_string("X-Forwarded-For"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, x_forwarded_for),
                 ngx_http_wasm_set_builtin_multi_header_handler },

#endif

#if (NGX_HTTP_REALIP)
    { ngx_string("X-Real-IP"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, x_real_ip),
                 ngx_http_wasm_set_builtin_header_handler },
#endif

#if (NGX_HTTP_DAV)
    { ngx_string("Depth"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, depth),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Destination"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, destination),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Overwrite"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, overwrite),
                 ngx_http_wasm_set_builtin_header_handler },

    { ngx_string("Date"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, date),
                 ngx_http_wasm_set_builtin_header_handler },
#endif

    { ngx_string("Cookie"),
                 offsetof(ngx_http_request_t, headers_in),
                 offsetof(ngx_http_headers_in_t, cookies),
                 ngx_http_wasm_set_builtin_multi_header_handler },

    { ngx_null_string, 0, 0, ngx_http_wasm_set_header_handler }
};


size_t
ngx_http_wasm_req_headers_count(ngx_http_request_t *r)
{
    return ngx_wasm_list_nelts(&r->headers_in.headers);
}


ngx_int_t
ngx_http_wasm_set_req_header(ngx_http_request_t *r, ngx_str_t key,
    ngx_str_t value, unsigned override)
{
    return ngx_http_wasm_set_header(r, NGX_HTTP_WASM_HEADER_REQUEST,
                                    ngx_http_wasm_special_req_headers,
                                    key, value, override);
}


/* handlers */


static ngx_int_t
ngx_http_wasm_set_builtin_multi_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    ngx_array_t       *headers;
    ngx_table_elt_t   *h, **ph;

    headers = (ngx_array_t *) ((char *) &r->headers_in + hv->offset);

    if (hv->override && headers->nelts) {

        /* clear */

        ngx_array_destroy(headers);

        if (ngx_array_init(headers, r->pool, 2,
                           sizeof(ngx_table_elt_t *))
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    if (headers->nalloc == 0) {
        if (ngx_array_init(headers, r->pool, 2,
                           sizeof(ngx_table_elt_t *))
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    if (ngx_http_wasm_set_header_helper(&r->headers_in.headers, hv, value, &h)
        == NGX_ERROR)
    {
        return NGX_ERROR;
    }

    if (!value->len) {
        return NGX_OK;
    }

    /* set */

    ph = ngx_array_push(headers);
    if (ph == NULL) {
        return NGX_ERROR;
    }

    *ph = h;

    return NGX_OK;
}


static ngx_int_t
ngx_http_wasm_set_host_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    r->headers_in.server = *value;

    return ngx_http_wasm_set_builtin_header_handler(r, hv, value);
}


static ngx_int_t
ngx_http_wasm_set_connection_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    size_t            i;
    ngx_table_elt_t  *h;
    ngx_list_part_t  *part;

    r->headers_in.connection_type = 0;

    if (value->len) {
        if (ngx_strcasestrn(value->data, "close", 5 - 1)) {
            r->headers_in.connection_type = NGX_HTTP_CONNECTION_CLOSE;
            r->headers_in.keep_alive_n = -1;

        } else if (ngx_strcasestrn(value->data, "keep-alive", 10 - 1)) {
            r->headers_in.connection_type = NGX_HTTP_CONNECTION_KEEP_ALIVE;
            r->keepalive = 1;
        }
    }

    /*
     * NOTE: nginx does not set r->headers_in.connection
     * so we do so here for ngx_http_wasm_set_builtin_header_handler
     * to update the existing value.
     */

    if (r->headers_in.connection == NULL) {

        part = &r->headers_in.headers.part;
        h = part->elts;

        for (i = 0; /* void */; i++) {

            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                h = part->elts;
                i = 0;
            }

            if (h[i].hash != 0
                && h[i].key.len == hv->key.len
                && ngx_strncasecmp(hv->key.data, h[i].key.data, h[i].key.len)
                                   == 0)
            {
                r->headers_in.connection = &h[i];
                break;
            }
        }
    }

    return ngx_http_wasm_set_builtin_header_handler(r, hv, value);
}


static ngx_int_t
ngx_http_wasm_set_ua_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    u_char  *user_agent, *msie;

    /* clear */

    r->headers_in.msie = 0;
    r->headers_in.msie6 = 0;
    r->headers_in.opera = 0;
    r->headers_in.gecko = 0;
    r->headers_in.chrome = 0;
    r->headers_in.safari = 0;
    r->headers_in.konqueror = 0;

    if (!value->len) {
        return ngx_http_wasm_set_builtin_header_handler(r, hv, value);
    }

    /* set */

    user_agent = value->data;

    msie = ngx_strstrn(user_agent, "MSIE ", 5 - 1);
    if (msie && msie + 7 < user_agent + value->len) {
        r->headers_in.msie = 1;

        if (msie[6] == '.') {
            switch (msie[5]) {
            case '4':
            case '5':
                r->headers_in.msie6 = 1;
                break;
            case '6':
                if (ngx_strstrn(msie + 8, "SV1", 3 - 1) == NULL) {
                    r->headers_in.msie6 = 1;
                }
                break;
            }
        }
    }

    if (ngx_strstrn(user_agent, "Opera", 5 - 1)) {
        r->headers_in.opera = 1;
        r->headers_in.msie = 0;
        r->headers_in.msie6 = 0;
    }

    if (!r->headers_in.msie && !r->headers_in.opera) {
        if (ngx_strstrn(user_agent, "Gecko/", 6 - 1)) {
            r->headers_in.gecko = 1;

        } else if (ngx_strstrn(user_agent, "Chrome/", 7 - 1)) {
            r->headers_in.chrome = 1;

        } else if (ngx_strstrn(user_agent, "Safari/", 7 - 1)
                   && ngx_strstrn(user_agent, "Mac OS X", 8 - 1))
        {
            r->headers_in.safari = 1;

        } else if (ngx_strstrn(user_agent, "Konqueror", 9 - 1)) {
            r->headers_in.konqueror = 1;
        }
    }

    return ngx_http_wasm_set_builtin_header_handler(r, hv, value);
}


static ngx_int_t
ngx_http_wasm_set_cl_header_handler(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    off_t   len;

    if (!value->len) {
        /* clear */
        r->headers_in.content_length_n = -1;
        return ngx_http_clear_builtin_header(r, hv, value);
    }

    len = ngx_atoof(value->data, value->len);
    if (len == NGX_ERROR) {
        ngx_wasm_log_error(NGX_LOG_ERR, r->connection->log, 0,
                           "attempt to set invalid Content-Length "
                           "request header: \"%V\"",
                           value);
        return NGX_ERROR;
    }

    /* set */

    r->headers_in.content_length_n = len;

    return ngx_http_wasm_set_builtin_header_handler(r, hv, value);
}


static ngx_int_t
ngx_http_clear_builtin_header(ngx_http_request_t *r,
    ngx_http_wasm_header_val_t *hv, ngx_str_t *value)
{
    value->len = 0;

    return ngx_http_wasm_set_builtin_header_handler(r, hv, value);
}