/**
 * @copyright
 * Copyright (c) 2016 Stanislav Ivochkin
 * Licensed under the MIT License:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <assert.h>
#include <http_parser.h>
#include <dfk/internal.h>
#include <dfk/internal/misc.h>
#include <dfk/http/protocol.h>
#include <dfk/http/request.h>
#include <dfk/http/response.h>


typedef struct dfk__http_parser_data_t {
  dfk_t* dfk;
  dfk_http_request_t* req;
  dfk_strmap_item_t* cheader; /* current header */
  int keepalive;
} dfk__http_parser_data_t;


static void dfk__http_parser_data_init(dfk__http_parser_data_t* pdata, dfk_t* dfk)
{
  pdata->dfk = dfk;
  pdata->req = NULL;
  pdata->cheader = NULL;
  pdata->keepalive = 1;
}


static int dfk__http_on_message_begin(http_parser* parser)
{
  dfk__http_parser_data_t* p = (dfk__http_parser_data_t*) parser->data;
  DFK_DBG(p->dfk, "{%p}", (void*) p->req);
  return 0;
}


static int dfk__http_on_url(http_parser* parser, const char* at, size_t size)
{
  dfk__http_parser_data_t* p = (dfk__http_parser_data_t*) parser->data;
  DFK_DBG(p->dfk, "{%p} %.*s", (void*) p->req, (int) size, at);
  dfk__buf_append(&p->req->url, at, size);
  return 0;
}


static int dfk__http_on_header_field(http_parser* parser, const char* at, size_t size)
{
  dfk__http_parser_data_t* p = (dfk__http_parser_data_t*) parser->data;
  DFK_DBG(p->dfk, "{%p} %.*s", (void*) p->req, (int) size, at);
  if (!p->cheader || p->cheader->value.data) {
    if (p->cheader) {
      dfk_strmap_insert(&p->req->headers, p->cheader);
    }
    p->cheader = dfk_arena_alloc(p->req->_request_arena, sizeof(dfk_strmap_item_t));
    /** @todo check p->cheader for NULL */
    dfk_strmap_item_init(p->cheader, NULL, 0, NULL, 0);
  }
  dfk__buf_append(&p->cheader->key, at, size);
  return 0;
}


static int dfk__http_on_header_value(http_parser* parser, const char* at, size_t size)
{
  dfk__http_parser_data_t* p = (dfk__http_parser_data_t*) parser->data;
  DFK_DBG(p->dfk, "{%p} %.*s", (void*) p->req, (int) size, at);
  dfk__buf_append(&p->cheader->value, at, size);
  return 0;
}


static int dfk__http_on_headers_complete(http_parser* parser)
{
  dfk__http_parser_data_t* p = (dfk__http_parser_data_t*) parser->data;
  DFK_DBG(p->dfk, "{%p}", (void*) p->req);
  if (p->cheader) {
    dfk_strmap_insert(&p->req->headers, p->cheader);
    p->cheader = NULL;
  }
  p->req->_headers_done = 1;
  p->keepalive = http_should_keep_alive(parser);
  return 0;
}


static int dfk__http_on_body(http_parser* parser, const char* at, size_t size)
{
  dfk__http_parser_data_t* p = (dfk__http_parser_data_t*) parser->data;
  DFK_DBG(p->dfk, "{%p} %llu bytes", (void*) p->req, (unsigned long long) size);
  dfk__buf_append(&p->req->_bodypart, at, size);
  return 0;
}


static int dfk__http_on_message_complete(http_parser* parser)
{
  dfk__http_parser_data_t* p = (dfk__http_parser_data_t*) parser->data;
  DFK_DBG(p->dfk, "{%p}", (void*) p->req);
  return 0;
}


static int dfk__http_on_chunk_header(http_parser* parser)
{
  dfk__http_parser_data_t* p = (dfk__http_parser_data_t*) parser->data;
  DFK_DBG(p->dfk, "{%p}", (void*) p->req);
  return 0;
}


static int dfk__http_on_chunk_complete(http_parser* parser)
{
  dfk__http_parser_data_t* p = (dfk__http_parser_data_t*) parser->data;
  DFK_DBG(p->dfk, "{%p}", (void*) p->req);
  return 0;
}


void dfk__http(dfk_coro_t* coro, dfk_tcp_socket_t* sock, dfk_http_t* http)
{
  assert(http);

  char buf[DFK_HTTP_HEADERS_BUFFER] = {0};

  /* Arena for per-connection data */
  dfk_arena_t connection_arena;
  dfk_arena_init(&connection_arena, http->dfk);

  /* A pointer to pdata is passed to http_parser callbacks */
  dfk__http_parser_data_t pdata;
  dfk__http_parser_data_init(&pdata, coro->dfk);

  /* Initialize http_parser instance */
  http_parser parser;
  http_parser_settings parser_settings;
  http_parser_init(&parser, HTTP_REQUEST);
  parser.data = &pdata;
  http_parser_settings_init(&parser_settings);
  parser_settings.on_message_begin = dfk__http_on_message_begin;
  parser_settings.on_url = dfk__http_on_url;
  parser_settings.on_status = NULL; /* on_status is response-only */
  parser_settings.on_header_field = dfk__http_on_header_field;
  parser_settings.on_header_value = dfk__http_on_header_value;
  parser_settings.on_headers_complete = dfk__http_on_headers_complete;
  parser_settings.on_body = dfk__http_on_body;
  parser_settings.on_message_complete = dfk__http_on_message_complete;
  parser_settings.on_chunk_header = dfk__http_on_chunk_header;
  parser_settings.on_chunk_complete = dfk__http_on_chunk_complete;

  /* Requests processed within this connection */
  size_t nrequests = 0;

  while (pdata.keepalive) {
    /* Arena for per-request data */
    dfk_arena_t request_arena;
    dfk_arena_init(&request_arena, http->dfk);

    dfk_http_request_t req;
    /** @todo check return value */
    dfk_http_request_init(&req, http->dfk, &request_arena, &connection_arena, sock);

    pdata.req = &req;

    /* Read headers first */
    while (!req._headers_done) {
      ssize_t nread = dfk_tcp_socket_read(sock, buf, sizeof(buf));
      if (nread < 0) {
        pdata.keepalive = 0;
        goto connection_broken;
      }
      DFK_DBG(http->dfk, "{%p} execute parser", (void*) http);
      http_parser_execute(&parser, &parser_settings, buf, nread);
      DFK_DBG(http->dfk, "{%p} parser step done", (void*) http);
    }

    /* Request pre-processing */
    DFK_DBG(http->dfk, "{%p} populate common headers", (void*) http);
    req.major_version = parser.http_major;
    req.minor_version = parser.http_minor;
    req.method = parser.method;
    req.user_agent = dfk_strmap_get(&req.headers, DFK_HTTP_USER_AGENT, sizeof(DFK_HTTP_USER_AGENT) - 1);
    req.host = dfk_strmap_get(&req.headers, DFK_HTTP_HOST, sizeof(DFK_HTTP_HOST) - 1);
    req.accept = dfk_strmap_get(&req.headers, DFK_HTTP_ACCEPT, sizeof(DFK_HTTP_ACCEPT) - 1);
    req.content_type = dfk_strmap_get(&req.headers, DFK_HTTP_CONTENT_TYPE, sizeof(DFK_HTTP_CONTENT_TYPE) - 1);
    {
      /* Parse and set content_length member */
      dfk_buf_t content_length = dfk_strmap_get(&req.headers, DFK_HTTP_CONTENT_LENGTH, sizeof(DFK_HTTP_CONTENT_LENGTH) - 1);
      if (content_length.size) {
        long long intval;
        int res = dfk__strtoll(content_length, NULL, 10, &intval);
        if (res != dfk_err_ok) {
          DFK_WARNING(http->dfk, "{%p} malformed value for \"" DFK_HTTP_CONTENT_LENGTH "\" header: %.*s",
              (void*) http, (int) content_length.size, content_length.data);
        } else {
          req.content_length = (size_t) intval;
        }
      }
    }

    DFK_DBG(http->dfk, "{%p} request parsing done, "
        "%s %.*s HTTP/%hu.%hu \"%.*s\"",
        (void*) http,
        http_method_str((enum http_method) req.method),
        (int) req.url.size, req.url.data,
        req.major_version,
        req.minor_version,
        (int) req.user_agent.size, req.user_agent.data);

    dfk_http_response_t resp;
    /** @todo check return value */
    dfk_http_response_init(&resp, http->dfk, &request_arena, &connection_arena, sock);

    int hres = http->_handler(http, &req, &resp);
    DFK_INFO(http->dfk, "{%p} http handler returned %d", (void*) http, hres);
    if (hres != dfk_err_ok) {
      resp.code = DFK_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (nrequests + 1 >= http->keepalive_requests) {
      DFK_INFO(http->dfk, "{%p} maximum number of keepalive requests (%llu) "
               "for connection {%p} has reached, close connection",
               (void*) http, (unsigned long long) http->keepalive_requests,
               (void*) sock);
      pdata.keepalive = 0;
    }

    dfk_buf_t connection = dfk_strmap_get(&resp.headers, DFK_HTTP_CONNECTION,
                                          sizeof(DFK_HTTP_CONNECTION));
    if (!strncmp(connection.data, "close", DFK_MIN(connection.size, 5))) {
      pdata.keepalive = 0;
    }

    /**
     * @todo Implement keepalive properly
     */
    pdata.keepalive = 0;

    dfk_http_response_flush_headers(&resp);

    ++nrequests;

connection_broken:
    /** @todo check for return value */
    dfk_http_response_free(&resp);
    dfk_http_request_free(&req);
    dfk_arena_free(&request_arena);
  }

  dfk_arena_free(&connection_arena);

  DFK_CALL_RVOID(http->dfk, dfk_tcp_socket_close(sock));
}

