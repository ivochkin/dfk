/**
 * @copyright
 * Copyright (c) 2016-2017 Stanislav Ivochkin
 * Licensed under the MIT License (see LICENSE)
 */

#include <assert.h>
#include <dfk/malloc.h>
#include <dfk/urlencoding.h>
#include <dfk/strtoll.h>
#include <dfk/error.h>
#include <dfk/http/server.h>
#include <dfk/http/request.h>
#include <dfk/internal/http/request.h>
#include <dfk/internal.h>

#define TO_BUFLIST_ITEM(expr) DFK_CONTAINER_OF((expr), buflist_item_t, hook)

/**
 * on_chunk_header and on_chunk_complete callbacks were introduced in 2.5
 */
#if HTTP_PARSER_VERSION_MAJOR >= 2 && HTTP_PARSER_VERSION_MINOR >= 5
#define HTTP_PARSER_HAVE_ON_CHUNK_HEADER 1
#define HTTP_PARSER_HAVE_ON_CHUNK_COMPLETE 1
#define HTTP_PARSER_MAX_CALLBACK_ERR_CODE HPE_CB_chunk_complete
#else
#define HTTP_PARSER_HAVE_ON_CHUNK_HEADER 0
#define HTTP_PARSER_HAVE_ON_CHUNK_COMPLETE 0
#define HTTP_PARSER_MAX_CALLBACK_ERR_CODE HPE_CB_message_complete
#endif

/**
 * on_status_complete have been renamed to on_status in 2.2
 */
#if HTTP_PARSER_VERSION_MAJOR >= 2 && HTTP_PARSER_VERSION_MINOR >= 2
#define HTTP_PARSER_HAVE_ON_STATUS 1
#define HTTP_PARSER_HAVE_ON_STATUS_COMPLETE 0
#else
#define HTTP_PARSER_HAVE_ON_STATUS 0
#define HTTP_PARSER_HAVE_ON_STATUS_COMPLETE 1
#endif


typedef struct buflist_item_t {
  dfk_list_hook_t hook;
  dfk_buf_t buf;
} buflist_item_t;

static ssize_t dfk__mocked_read(dfk_http_request_t* req,
    char* buf, size_t toread)
{
#if DFK_MOCKS
  if (req->_socket_mocked) {
    return dfk__sponge_read(req->_socket_mock, buf, toread);
  } else {
    return dfk_tcp_socket_read(req->_socket, buf, toread);
  }
#else
  return dfk_tcp_socket_read(req->_socket, buf, toread);
#endif
}

static int dfk__http_request_allocate_headers_buf(dfk_http_request_t* req, dfk_buf_t* outbuf)
{
  assert(req);
  assert(outbuf);
  char* buf = dfk__malloc(req->http->dfk, req->http->headers_buffer_size);
  if (!buf) {
    return dfk_err_nomem;
  }
  *outbuf = (dfk_buf_t) {buf, req->http->headers_buffer_size};

  buflist_item_t* buflist_item =
      dfk_arena_alloc(req->_request_arena, sizeof(buflist_item_t));

  if (!buflist_item) {
    dfk__free(req->http->dfk, buf);
    return dfk_err_nomem;
  }
  buflist_item->buf = *outbuf;
  dfk_list_hook_init(&buflist_item->hook);
  dfk_list_append(&req->_buffers, &buflist_item->hook);
  return dfk_err_ok;
}

void dfk__http_request_init(dfk_http_request_t* req, dfk_http_t* http,
    dfk_arena_t* request_arena, dfk_arena_t* connection_arena,
    dfk_tcp_socket_t* sock)
{
  assert(req);
  assert(request_arena);
  assert(connection_arena);
  assert(sock);

  memset(req, 0, sizeof(dfk_http_request_t));
  req->_request_arena = request_arena;
  req->_connection_arena = connection_arena;
  req->_socket = sock;
  dfk_list_init(&req->_buffers);
  dfk_strmap_init(&req->headers);
  dfk_strmap_init(&req->arguments);
  req->http = http;
  req->content_length = -1;
}

void dfk__http_request_free(dfk_http_request_t* req)
{
  dfk_list_it it, end;
  dfk_list_begin(&req->_buffers, &it);
  dfk_list_end(&req->_buffers, &end);
  while (!dfk_list_it_equal(&it, &end)) {
    buflist_item_t* blitem = TO_BUFLIST_ITEM(it.value);
    dfk__free(req->http->dfk, blitem->buf.data);
    dfk_list_it_next(&it);
  }
}

size_t dfk_http_request_sizeof(void)
{
  return sizeof(dfk_http_request_t);
}

typedef struct dfk_header_parser_data_t {
  dfk_http_request_t* req;
  dfk_cbuf_t cheader_field;
  dfk_cbuf_t cheader_value;
  int dfk_errno;
} dfk_header_parser_data_t;

typedef struct dfk_body_parser_data_t {
  dfk_http_request_t* req;
  dfk_buf_t outbuf;
  int dfk_errno;
} dfk_body_parser_data_t;

static int dfk_http_request_on_message_begin(http_parser* parser)
{
  dfk_header_parser_data_t* p = (dfk_header_parser_data_t*) parser->data;
  DFK_DBG(p->req->http->dfk, "{%p}", (void*) p->req);
  return 0;
}

static int dfk_http_request_on_url(http_parser* parser, const char* at, size_t size)
{
  dfk_header_parser_data_t* p = (dfk_header_parser_data_t*) parser->data;
  DFK_DBG(p->req->http->dfk, "{%p} %.*s", (void*) p->req, (int) size, at);
  dfk_buf_append(&p->req->url, at, size);
  if (p->req->url.size > DFK_HTTP_HEADER_MAX_SIZE) {
    p->dfk_errno = dfk_err_overflow;
    return 1;
  }
  return 0;
}

static int dfk__http_request_flush_header(dfk_header_parser_data_t* p)
{
  assert(p);
  dfk_strmap_item_t* item = dfk_arena_alloc(p->req->_request_arena, sizeof(dfk_strmap_item_t));
  if (!item) {
    p->dfk_errno = dfk_err_nomem;
    return 1;
  }
  dfk_strmap_item_init(item, p->cheader_field.data, p->cheader_field.size,
      (char*) p->cheader_value.data, p->cheader_value.size);
  dfk_strmap_insert(&p->req->headers, item);
  return 0;
}

static int dfk_http_request_on_header_field(http_parser* parser, const char* at, size_t size)
{
  dfk_header_parser_data_t* p = (dfk_header_parser_data_t*) parser->data;
  DFK_DBG(p->req->http->dfk, "{%p} %.*s", (void*) p->req, (int) size, at);
  if (p->cheader_value.data) {
    if (dfk__http_request_flush_header(p)) {
      return 1;
    }
    p->cheader_value.data = NULL;
    p->cheader_field = (dfk_cbuf_t) {.data = at, .size = size};
  } else {
    dfk_cbuf_append(&p->cheader_field, at, size);
  }
  if (p->cheader_field.size > DFK_HTTP_HEADER_MAX_SIZE) {
    p->dfk_errno = dfk_err_overflow;
    return 1;
  }
  return 0;
}

static int dfk_http_request_on_header_value(http_parser* parser, const char* at, size_t size)
{
  dfk_header_parser_data_t* p = (dfk_header_parser_data_t*) parser->data;
  DFK_DBG(p->req->http->dfk, "{%p} %.*s", (void*) p->req, (int) size, at);
  if (!p->cheader_value.data) {
    p->cheader_value = (dfk_cbuf_t) {.data = at, .size = size};
  } else {
    dfk_cbuf_append(&p->cheader_value, at, size);
  }
  if (p->cheader_value.size > DFK_HTTP_HEADER_MAX_SIZE) {
    p->dfk_errno = dfk_err_overflow;
    return 1;
  }
  return 0;
}

static int dfk_http_request_on_headers_complete(http_parser* parser)
{
  dfk_header_parser_data_t* p = (dfk_header_parser_data_t*) parser->data;
  DFK_DBG(p->req->http->dfk, "{%p}", (void*) p->req);
  if (p->cheader_field.data) {
    assert(p->cheader_value.data);
    dfk__http_request_flush_header(p);
  }
  http_parser_pause(parser, 1);
  return 0;
}

static int dfk_http_request_on_body(http_parser* parser, const char* at, size_t size)
{
  dfk_body_parser_data_t* p = (dfk_body_parser_data_t*) parser->data;
  DFK_DBG(p->req->http->dfk, "{%p} %llu bytes", (void*) p->req, (unsigned long long) size);
  memmove(p->outbuf.data, at, size);
  assert(size <= p->outbuf.size);
  p->outbuf.data += size;
  p->outbuf.size -= size;
  p->req->_body_nread += size;
  return 0;
}

static int dfk_http_request_on_message_complete(http_parser* parser)
{
  dfk_header_parser_data_t* p = (dfk_header_parser_data_t*) parser->data;
  /*
   * Use only p->req field below, since a pointer to dfk_body_parser_data_t
   * could be stored in parser->data as well
   */
  DFK_DBG(p->req->http->dfk, "{%p}", (void*) p->req);
  http_parser_pause(parser, 1);
  return 0;
}

#if HTTP_PARSER_HAVE_ON_CHUNK_HEADER
static int dfk_http_request_on_chunk_header(http_parser* parser)
{
  dfk_header_parser_data_t* p = (dfk_header_parser_data_t*) parser->data;
  DFK_DBG(p->req->http->dfk, "{%p}, chunk size %llu", (void*) p->req,
      (unsigned long long) parser->content_length);
  return 0;
}
#endif /* HTTP_PARSER_HAVE_ON_CHUNK_HEADER */

#if HTTP_PARSER_HAVE_ON_CHUNK_COMPLETE
static int dfk_http_request_on_chunk_complete(http_parser* parser)
{
  dfk_header_parser_data_t* p = (dfk_header_parser_data_t*) parser->data;
  DFK_DBG(p->req->http->dfk, "{%p}", (void*) p->req);
  return 0;
}
#endif /* HTTP_PARSER_HAVE_ON_CHUNK_COMPLETE */

static http_parser_settings dfk_parser_settings = {
  .on_message_begin = dfk_http_request_on_message_begin,
  .on_url = dfk_http_request_on_url,
#if HTTP_PARSER_HAVE_ON_STATUS
  .on_status = NULL, /* For HTTP responses only */
#endif
#if HTTP_PARSER_HAVE_ON_STATUS_COMPLETE
  .on_status_complete = NULL, /* For HTTP responses only */
#endif
  .on_header_field = dfk_http_request_on_header_field,
  .on_header_value = dfk_http_request_on_header_value,
  .on_headers_complete = dfk_http_request_on_headers_complete,
  .on_body = dfk_http_request_on_body,
  .on_message_complete = dfk_http_request_on_message_complete,
#if HTTP_PARSER_HAVE_ON_CHUNK_HEADER
  .on_chunk_header = dfk_http_request_on_chunk_header,
#endif
#if HTTP_PARSER_HAVE_ON_CHUNK_COMPLETE
  .on_chunk_complete = dfk_http_request_on_chunk_complete,
#endif
};

static int dfk_http_request_add_argument(dfk_http_request_t* req,
    const char* key_begin, const char* key_end,
    char* value_begin, const char* value_end)
{
  assert(key_begin <= key_end);
  assert(value_begin <= value_end);
  DFK_DBG(req->http->dfk, "{%p} key:'%.*s', value:'%.*s'", (void*) req,
      (int) (key_end - key_begin), key_begin,
      (int) (value_end - value_begin), value_begin);
  dfk_strmap_item_t* item = dfk_arena_alloc(req->_request_arena,
      sizeof(dfk_strmap_item_t));
  if (!item) {
    return dfk_err_nomem;
  }
  dfk_strmap_item_init(item, key_begin, key_end - key_begin,
      value_begin, value_end - value_begin);
  dfk_strmap_insert(&req->arguments, item);
  return dfk_err_ok;
}

int dfk__http_request_read_headers(dfk_http_request_t* req)
{
  assert(req);
  assert(req->http);

  dfk_t* dfk = req->http->dfk;

  /* A pointer to pdata is passed to http_parser callbacks */
  dfk_header_parser_data_t pdata = {
    .req = req,
    .cheader_field = {NULL, 0},
    .cheader_value = {NULL, 0},
    .dfk_errno = dfk_err_ok
  };

  /* Initialize http_parser instance */
  http_parser_init(&req->_parser, HTTP_REQUEST);
  req->_parser.data = &pdata;

  /* Allocate first per-request buffer */
  dfk_buf_t curbuf;
  DFK_CALL(dfk, dfk__http_request_allocate_headers_buf(req, &curbuf));

  while (1) {
    assert(curbuf.size >= DFK_HTTP_HEADER_MAX_SIZE);

    ssize_t nread = dfk__mocked_read(req, curbuf.data, curbuf.size);
    if (nread <= 0) {
      return dfk_err_eof;
    }

    DFK_DBG(dfk, "{%p} http parse bytes: %llu",
        (void*) req, (unsigned long long) nread);

    /*
     * Invoke http_parser.
     * Note that size_t -> ssize_t cast occurs in the line below
     */
    ssize_t nparsed = http_parser_execute(
        &req->_parser, &dfk_parser_settings, curbuf.data, curbuf.size);
    assert(nparsed <= nread);
    DFK_DBG(dfk, "{%p} %llu bytes parsed",
        (void*) req, (unsigned long long) nparsed);
    DFK_DBG(dfk, "{%p} http parser returned %d (%s) - %s",
        (void*) req, req->_parser.http_errno,
        http_errno_name(req->_parser.http_errno),
        http_errno_description(req->_parser.http_errno));

    if (req->_parser.http_errno == HPE_PAUSED) {
      /* on_headers_complete was called */
      req->_remainder = (dfk_buf_t) {curbuf.data + nparsed, nread - nparsed};
      DFK_DBG(dfk, "{%p} all headers parsed, remainder %llu bytes",
          (void*) req, (unsigned long long) req->_remainder.size);
      http_parser_pause(&req->_parser, 0);
      break;
    }
    if (HPE_CB_message_begin <= req->_parser.http_errno
        && req->_parser.http_errno <= HTTP_PARSER_MAX_CALLBACK_ERR_CODE) {
      DFK_DBG(dfk, "{%p} http parser returned error %u",
          (void*) req, req->_parser.http_errno);
      return pdata.dfk_errno;
    }
    if (req->_parser.http_errno != HPE_OK) {
      DFK_DBG(dfk, "{%p} http parser returned error %u",
          (void*) req, req->_parser.http_errno);
      return dfk_err_protocol;
    }

    /*
     * If the buffer's remainder isn't large enough to hold up to DFK_HTTP_HEADER_MAX_SIZE
     * bytes, which is the maximum size of the current header, one have to reallocate current
     * buffer.
     */
    if (pdata.cheader_field.data &&
        curbuf.data + curbuf.size - pdata.cheader_field.data < DFK_HTTP_HEADER_MAX_SIZE) {
      if (dfk_list_size(&req->_buffers) == req->http->headers_buffer_count) {
        return dfk_err_overflow;
      }
      DFK_CALL(dfk, dfk__http_request_allocate_headers_buf(req, &curbuf));
      memcpy(curbuf.data, pdata.cheader_field.data, nparsed);
      if (pdata.cheader_value.data) {
        pdata.cheader_value.data = curbuf.data +
          (pdata.cheader_value.data - pdata.cheader_field.data);
      }
      pdata.cheader_field.data = curbuf.data;
    }
    curbuf = (dfk_buf_t) {curbuf.data + nparsed, curbuf.size - nparsed};
  }

  /* Request pre-processing */
  DFK_DBG(dfk, "{%p} set version = %d.%d and method = %s",
      (void*) req, req->_parser.http_major, req->_parser.http_minor,
      http_method_str(req->_parser.method));
  req->major_version = req->_parser.http_major;
  req->minor_version = req->_parser.http_minor;
  req->method = req->_parser.method;
  DFK_DBG(dfk, "{%p} populate common headers", (void*) req);
  req->user_agent = dfk_strmap_get(&req->headers,
      DFK_HTTP_USER_AGENT, sizeof(DFK_HTTP_USER_AGENT) - 1);
  DFK_DBG(dfk, "{%p} user-agent: \"%.*s\"", (void*) req,
      (int) req->user_agent.size, req->user_agent.data);
  req->host = dfk_strmap_get(&req->headers,
      DFK_HTTP_HOST, sizeof(DFK_HTTP_HOST) - 1);
  DFK_DBG(dfk, "{%p} host: \"%.*s\"", (void*) req,
      (int) req->host.size, req->host.data);
  req->accept = dfk_strmap_get(&req->headers,
      DFK_HTTP_ACCEPT, sizeof(DFK_HTTP_ACCEPT) - 1);
  DFK_DBG(dfk, "{%p} accept: \"%.*s\"", (void*) req,
      (int) req->accept.size, req->accept.data);
  req->content_type = dfk_strmap_get(&req->headers,
      DFK_HTTP_CONTENT_TYPE, sizeof(DFK_HTTP_CONTENT_TYPE) - 1);
  DFK_DBG(dfk, "{%p} content-type: \"%.*s\"", (void*) req,
      (int) req->content_type.size, req->content_type.data);
  dfk_buf_t content_length = dfk_strmap_get(&req->headers,
      DFK_HTTP_CONTENT_LENGTH, sizeof(DFK_HTTP_CONTENT_LENGTH) - 1);
  DFK_DBG(dfk, "{%p} parse content length \"%.*s\"", (void*) req, (int) content_length.size, content_length.data);
  if (content_length.size) {
    long long intval;
    int res = dfk_strtoll(content_length, NULL, 10, &intval);
    if (res != dfk_err_ok) {
      DFK_WARNING(dfk, "{%p} malformed value for \"" DFK_HTTP_CONTENT_LENGTH "\" header: %.*s",
          (void*) req, (int) content_length.size, content_length.data);
    } else {
      req->content_length = intval;
    }
  }
  req->keepalive = http_should_keep_alive(&req->_parser);
  req->chunked = !!(req->_parser.flags & F_CHUNKED);
  if (req->content_length == -1 && !req->chunked) {
    DFK_DBG(dfk, "{%p} no content-length header, no chunked encoding - "
        "expecting empty body", (void*) req);
    req->content_length = 0;
  }
  DFK_DBG(dfk, "{%p} keepalive: %d, chunked encoding: %d",
      (void*) req, req->keepalive, req->chunked);

  DFK_DBG(dfk, "{%p} parse url '%.*s'", (void*) req,
      (int) req->url.size, req->url.data);
  struct http_parser_url urlparser;
  int err = http_parser_parse_url(req->url.data, req->url.size, 0, &urlparser);
  if (err) {
    DFK_ERROR(dfk, "{%p} url parser returned %d during parsing url \"%.*s\"",
        (void*) req, err, (int) req->url.size, req->url.data);
    return dfk_err_protocol;
  }
  dfk_buf_t* fields[] = {
    NULL, /* ignore schema */
    NULL, /* ignore host */
    NULL, /* ignore port */
    &req->path,
    &req->query,
    &req->fragment,
    NULL, /* ignore userinfo */
  };
  assert(DFK_SIZE(fields) == UF_MAX);
  for (int i = 0; i < UF_MAX; ++i) {
    if (fields[i] && urlparser.field_set & (1 << i)) {
      fields[i]->data = req->url.data + urlparser.field_data[i].off;
      fields[i]->size = urlparser.field_data[i].len;
    }
  }

  /* Parse query */
  DFK_DBG(dfk, "{%p} parse query '%.*s'", (void*) req,
      (int) req->query.size, req->query.data);
  if (req->query.size) {
    char* decoded_query = dfk_arena_alloc(req->_request_arena, req->query.size);
    if (!decoded_query) {
      return dfk_err_nomem;
    }
    size_t decoded_query_size;
    size_t bytesdecoded = dfk_urldecode(req->query.data, req->query.size,
        decoded_query, &decoded_query_size);
    if (bytesdecoded != req->query.size) {
      return dfk_err_protocol;
    }
    DFK_DBG(dfk, "{%p} url-decoded: '%.*s", (void*) req,
        (int) decoded_query_size, decoded_query);
    /*
     * State, can be one of
     *  0 - initial state
     *  1 - reading key
     *  2 - observing '=' sign
     *  3 - reading value
     */
    int state = 0;
    char* decoded_query_end = decoded_query + decoded_query_size;
    char* key_begin = NULL;
    char* key_end = NULL;
    char* value_begin = NULL;

    for (char* i = decoded_query; i != decoded_query_end; ++i) {
      switch (state) {
        case 0: /* Initial state */
          key_begin = i;
          state = 1;
          break;
        case 1: /* Reading key */
          if (*i == '=') {
            key_end = i;
            state = 2;
          }
          break;
        case 2: /* Observing '=' sign */
          value_begin = i;
          state = 3;
          break;
        case 3: /* Reading value */
          if (*i == '&') {
            err = dfk_http_request_add_argument(req, key_begin, key_end, value_begin, i);
            if (err != dfk_err_ok) {
              DFK_ERROR(dfk, "{%p} dfk_http_request_add_argument failed with code %d",
                  (void*) req, err);
              return err;
            }
            key_begin = NULL;
            state = 0;
          }
          break;
        default:
          assert(0 && "Impossible state for HTTP query parsing");
      }
    }
    if (state != 0 && state != 3) {
      DFK_ERROR(dfk, "{%p} bad query string, "
          "final state after parsing is %d (0 or 3 expected)",
          (void*) req, state);
      return dfk_err_protocol;
    }
    if (state == 3) {
      err = dfk_http_request_add_argument(req,
          key_begin, key_end, value_begin, decoded_query_end);
      if (err != dfk_err_ok) {
        DFK_ERROR(dfk, "{%p} dfk_http_request_add_argument failed with code %d",
            (void*) req, err);
        return err;
      }
    }
  }
  return dfk_err_ok;
}

ssize_t dfk_http_request_read(dfk_http_request_t* req, char* buf, size_t size)
{
  if (!req) {
    return -1;
  }

  if(!buf && size) {
    req->http->dfk->dfk_errno = dfk_err_badarg;
    return -1;
  }

  DFK_DBG(req->http->dfk, "{%p} bytes requested: %llu",
      (void*) req, (unsigned long long) size);

  if (!req->content_length && !req->chunked) {
    DFK_WARNING(req->http->dfk, "{%p} can not read request body without "
        "content-length or transfer-encoding: chunked", (void*) req);
    req->http->dfk->dfk_errno = dfk_err_eof;
    return -1;
  }

  char* bufcopy = buf;
  size_t sizecopy = size;
  DFK_UNUSED(sizecopy);
  dfk_body_parser_data_t pdata;
  pdata.req = req;
  pdata.dfk_errno = dfk_err_ok;
  pdata.outbuf = (dfk_buf_t) {buf, size};

  while (size && (pdata.outbuf.data == bufcopy || req->_remainder.size)) {
    size_t toread = DFK_MIN(size, req->content_length - req->_body_nread);
    DFK_DBG(req->http->dfk, "{%p} bytes cached: %llu, user-provided buffer used: %llu/%llu bytes",
        (void*) req, (unsigned long long) req->_remainder.size,
        (unsigned long long) (pdata.outbuf.data - bufcopy),
        (unsigned long long) sizecopy);
    DFK_DBG(req->http->dfk, "{%p} will read %llu bytes", (void*) req, (unsigned long long) toread);
    if (size > toread) {
      DFK_WARNING(req->http->dfk, "{%p} requested more bytes than could be read from "
          "request, content-length: %llu, already read: %llu, requested: %llu",
          (void*) req, (unsigned long long) req->content_length,
          (unsigned long long) req->_body_nread,
          (unsigned long long) size);
    }

    dfk_buf_t inbuf;
    /*
     * prepare inbuf - use either bytes cached in req->_remainder,
     * or read from req->_socket
     */
    if (req->_remainder.size) {
      toread = DFK_MIN(toread, req->_remainder.size);
      inbuf = (dfk_buf_t) {req->_remainder.data, toread};
    } else {
      DFK_DBG(req->http->dfk, "{%p} cache is empty, read new bytes", (void*) req);
      size_t nread = dfk__mocked_read(req, buf, toread);
      if (nread <= 0) {
        /* preserve dfk->dfk_errno from dfk_tcp_socket_read or dfk__sponge_read */
        return nread;
      }
      inbuf = (dfk_buf_t) {buf, nread};
    }

    assert(inbuf.data);
    assert(inbuf.size > 0);
    req->_parser.data = &pdata;
    DFK_DBG(req->http->dfk, "{%p} http parse bytes: %llu",
        (void*) req, (unsigned long long) inbuf.size);
    size_t nparsed = http_parser_execute(
        &req->_parser, &dfk_parser_settings, inbuf.data, inbuf.size);
    DFK_DBG(req->http->dfk, "{%p} %llu bytes parsed",
        (void*) req, (unsigned long long) nparsed);
    DFK_DBG(req->http->dfk, "{%p} http parser returned %d (%s) - %s",
        (void*) req, req->_parser.http_errno,
        http_errno_name(req->_parser.http_errno),
        http_errno_description(req->_parser.http_errno));
    if (req->_parser.http_errno != HPE_PAUSED
        && req->_parser.http_errno != HPE_OK) {
      return dfk_err_protocol;
    }
    assert(nparsed <= inbuf.size);
    if (req->_remainder.size) {
      /* bytes from remainder were parsed */
      req->_remainder.data += nparsed;
      req->_remainder.size -= nparsed;
    }
    buf = pdata.outbuf.data;
    size = pdata.outbuf.size;
  }
  DFK_DBG(req->http->dfk, "%llu", (unsigned long long ) (pdata.outbuf.data - bufcopy));
  return pdata.outbuf.data - bufcopy;
}

ssize_t dfk_http_request_readv(dfk_http_request_t* req, dfk_iovec_t* iov, size_t niov)
{
  if (!req || (!iov && niov)) {
    return dfk_err_badarg;
  }
  DFK_DBG(req->http->dfk, "{%p} into %llu blocks", (void*) req, (unsigned long long) niov);
  return dfk_http_request_read(req, iov[0].data, iov[0].size);
}

