/**
 * @file dfk/http.h
 * HTTP server
 *
 * @copyright
 * Copyright (c) 2016, Stanislav Ivochkin. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <dfk/config.h>
#include <dfk/tcp_socket.h>
#include <dfk/internal/arena.h>
#include <dfk/internal/avltree.h>


/**
 * @addtogroup http http
 * @{
 */


typedef enum dfk_http_method_e {
  DFK_HTTP_DELETE = 0,
  DFK_HTTP_GET = 1,
  DFK_HTTP_HEAD = 2,
  DFK_HTTP_POST = 3,
  DFK_HTTP_PUT = 4,
  DFK_HTTP_CONNECT = 5,
  DFK_HTTP_OPTIONS = 6,
  DFK_HTTP_TRACE = 7,
  DFK_HTTP_COPY = 8,
  DFK_HTTP_LOCK = 9,
  DFK_HTTP_MKCOL = 10,
  DFK_HTTP_MOVE = 11,
  DFK_HTTP_PROPFIND = 12,
  DFK_HTTP_PROPPATCH = 13,
  DFK_HTTP_SEARCH = 14,
  DFK_HTTP_UNLOCK = 15,
  DFK_HTTP_BIND = 16,
  DFK_HTTP_REBIND = 17,
  DFK_HTTP_UNBIND = 18,
  DFK_HTTP_ACL = 19,
  DFK_HTTP_REPORT = 20,
  DFK_HTTP_MKACTIVITY = 21,
  DFK_HTTP_CHECKOUT = 22,
  DFK_HTTP_MERGE = 23,
  DFK_HTTP_MSEARCH = 24,
  DFK_HTTP_NOTIFY = 25,
  DFK_HTTP_SUBSCRIBE = 26,
  DFK_HTTP_UNSUBSCRIBE = 27,
  DFK_HTTP_PATCH = 28,
  DFK_HTTP_PURGE = 29,
  DFK_HTTP_MKCALENDAR = 30,
  DFK_HTTP_LINK = 31,
  DFK_HTTP_UNLINK = 32
} dfk_http_method_e;


typedef enum dfk_http_status_e {
  DFK_HTTP_CONTINUE = 100,
  DFK_HTTP_SWITCHING_PROTOCOLS = 101,
  DFK_HTTP_PROCESSING = 102, /* WebDAV, RFC 2518 */

  DFK_HTTP_OK = 200,
  DFK_HTTP_CREATED = 201,
  DFK_HTTP_ACCEPTED = 202,
  DFK_HTTP_NON_AUTHORITATIVE_INFORMATION = 203,
  DFK_HTTP_NO_CONTENT = 204 ,
  DFK_HTTP_RESET_CONTENT = 205,
  DFK_HTTP_PARTIAL_CONTENT = 206, /* RFC 7233 */
  DFK_HTTP_MULTI_STATUS = 207, /* WebDAV, RFC 4918 */
  DFK_HTTP_ALREADY_REPORTED = 208, /* WebDAV, RFC 5842 */
  DFK_HTTP_IM_USED = 226, /* RFC 3229 */

  DFK_HTTP_MULTIPLE_CHOICES = 300,
  DFK_HTTP_MOVED_PERMANENTLY = 301,
  DFK_HTTP_FOUND = 302,
  DFK_HTTP_SEE_OTHER = 303,
  DFK_HTTP_NOT_MODIFIED = 304, /* RFC 7232 */
  DFK_HTTP_USE_PROXY = 305,
  DFK_HTTP_SWITCH_PROXY = 306,
  DFK_HTTP_TEMPORARY_REDIRECT = 307,
  DFK_HTTP_PERMANENT_REDIRECT = 308, /* RFC 7538 */

  DFK_HTTP_BAD_REQUEST = 400,
  DFK_HTTP_UNAUTHORIZED = 401, /* RFC 7235 */
  DFK_HTTP_PAYMENT_REQUIRED = 402,
  DFK_HTTP_FORBIDDEN = 403,
  DFK_HTTP_NOT_FOUND = 404,
  DFK_HTTP_METHOD_NOT_ALLOWED = 405,
  DFK_HTTP_NOT_ACCEPTABLE = 406,
  DFK_HTTP_PROXY_AUTHENTICATION_REQUIRED = 407, /* RFC 7235 */
  DFK_HTTP_REQUEST_TIMEOUT = 408,
  DFK_HTTP_CONFLICT = 409,
  DFK_HTTP_GONE = 410,
  DFK_HTTP_LENGTH_REQUIRED = 411,
  DFK_HTTP_PRECONDITION_FAILED = 412, /* RFC 7232 */
  DFK_HTTP_PAYLOAD_TOO_LARGE = 413, /* RFC 7231 */
  DFK_HTTP_URI_TOO_LONG = 414, /* RFC 7231 */
  DFK_HTTP_UNSUPPORTED_MEDIA_TYPE = 415,
  DFK_HTTP_RANGE_NOT_SATISFIABLE = 416, /* RFC 7233 */
  DFK_HTTP_EXPECTATION_FAILED = 417,
  DFK_HTTP_I_AM_A_TEAPOT = 418, /* RFC 2324 */
  DFK_HTTP_MISDIRECTED_REQUEST = 421, /* RFC 7540 */
  DFK_HTTP_UNPROCESSABLE_ENTITY = 422, /* WebDAV; RFC 4918 */
  DFK_HTTP_LOCKED = 423, /* WebDAV; RFC 4918 */
  DFK_HTTP_FAILED_DEPENDENCY = 424, /* WebDAV; RFC 4918 */
  DFK_HTTP_UPGRADE_REQUIRED = 426,
  DFK_HTTP_PRECONDITION_REQUIRED = 428, /* RFC 6585 */
  DFK_HTTP_TOO_MANY_REQUESTS = 429, /* RFC 6585 */
  DFK_HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE = 431, /* RFC 6585 */
  DFK_HTTP_UNAVAILABLE_FOR_LEGAL_REASONS = 451,

  DFK_HTTP_INTERNAL_SERVER_ERROR = 500,
  DFK_HTTP_NOT_IMPLEMENTED = 501,
  DFK_HTTP_BAD_GATEWAY = 502,
  DFK_HTTP_SERVICE_UNAVAILABLE = 503,
  DFK_HTTP_GATEWAY_TIMEOUT = 504,
  DFK_HTTP_HTTP_VERSION_NOT_SUPPORTED = 505,
  DFK_HTTP_VARIANT_ALSO_NEGOTIATES = 506, /* RFC 2295 */
  DFK_HTTP_INSUFFICIENT_STORAGE = 507, /* WebDAV; RFC 4918 */
  DFK_HTTP_LOOP_DETECTED = 508, /* WebDAV; RFC 5842 */
  DFK_HTTP_NOT_EXTENDED = 510, /* RFC 2774 */
  DFK_HTTP_NETWORK_AUTHENTICATION_REQUIRED = 511 /* RFC 6585 */
} dfk_http_status_e;


/**
 * HTTP request type
 */
typedef struct dfk_http_req_t {
  /**
   * @privatesection
   */
  dfk_arena_t* _arena;
  dfk_tcp_socket_t* _sock;
  dfk_avltree_t _headers; /* contains dfk_http_header_t */
  dfk_avltree_t _arguments; /* contains dfk_http_argument_t */
  dfk_buf_t _bodypart;
  size_t _body_bytes_nread;
  int _headers_done;

  /**
   * @publicsection
   */

  unsigned short major_version;
  unsigned short minor_version;

  dfk_http_method_e method;
  dfk_buf_t url;
  dfk_buf_t user_agent;
  dfk_buf_t host;
  dfk_buf_t accept;
  dfk_buf_t content_type;
  uint64_t content_length;
} dfk_http_req_t;


typedef struct dfk_http_resp_t {
  /**
   * @privatesection
   */
  dfk_arena_t* _arena;
  dfk_tcp_socket_t* _sock;
  dfk_avltree_t _headers;

  /**
   * @publicsection
   */

  dfk_http_status_e code;
} dfk_http_resp_t;


typedef struct dfk_http_headers_it {
  /** @private */
  dfk_avltree_it_t _it;
  dfk_buf_t field;
  dfk_buf_t value;
} dfk_http_headers_it;


typedef dfk_http_headers_it dfk_http_args_it;


ssize_t dfk_http_read(dfk_http_req_t* req, char* buf, size_t size);
ssize_t dfk_http_readv(dfk_http_req_t* req, dfk_iovec_t* iov, size_t niov);

dfk_buf_t dfk_http_get(dfk_http_req_t* req, const char* name, size_t namesize);
int dfk_http_headers_begin(dfk_http_req_t* req, dfk_http_headers_it* it);
int dfk_http_headers_next(dfk_http_headers_it* it);
int dfk_http_headers_valid(dfk_http_headers_it* it);

dfk_buf_t dfk_http_getarg(dfk_http_req_t* req, const char* name, size_t namesize);
int dfk_http_args_begin(dfk_http_req_t* req, dfk_http_args_it* it);
int dfk_http_args_next(dfk_http_args_it* it);
int dfk_http_args_end(dfk_http_args_it* it);

ssize_t dfk_http_write(dfk_http_resp_t* resp, char* buf, size_t nbytes);
ssize_t dfk_http_writev(dfk_http_resp_t* resp, dfk_iovec_t* iov, size_t niov);
int dfk_http_set(dfk_http_resp_t* resp, const char* name, size_t namesize, const char* value, size_t valuesize);


struct dfk_http_t;
typedef int (*dfk_http_handler)(struct dfk_http_t*, dfk_http_req_t*, dfk_http_resp_t*);


typedef struct dfk_http_t {
  /**
   * @privatesection
   */

  dfk_list_hook_t _hook;
  dfk_tcp_socket_t _listensock;
  dfk_http_handler _handler;
  dfk_list_t _connections;

  /**
   * @publicsection
   */

  dfk_t* dfk;
  dfk_userdata_t user;

  /**
   * Maximum number of requests for a single keepalive connection
   * @note default: 100
   */
  size_t keepalive_requests;
} dfk_http_t;


int dfk_http_init(dfk_http_t* http, dfk_t* dfk);
int dfk_http_stop(dfk_http_t* http);
int dfk_http_free(dfk_http_t* http);
int dfk_http_serve(dfk_http_t* http,
    const char* endpoint,
    uint16_t port,
    dfk_http_handler handler);

/** @} */

