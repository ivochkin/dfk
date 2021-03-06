/**
 * @copyright
 * Copyright (c) 2015-2017 Stanislav Ivochkin
 * Licensed under the MIT License (see LICENSE)
 */

#include <stdlib.h>
#include <dfk/http/request.h>
#include <dfk/internal/http/request.h>
#include <dfk/http/server.h>
#include <ut.h>

/*
 * The following tests are using dfk_http_request._sock mock,
 * so if DFK_MOCKS are disabled, these tests does not make sense
 */
#if DFK_MOCKS


typedef struct fixture_t {
  dfk_t dfk;
  dfk_arena_t conn_arena;
  dfk_arena_t req_arena;
  dfk_http_t http;
  dfk_tcp_socket_t sock;
  dfk_http_request_t req;
  dfk__sponge_t reqbuf;
} fixture_t;


static void fixture_setup(fixture_t* f)
{
  dfk_init(&f->dfk);
  /*
   * We don't call dfk_http_init here because we are outside of
   * dfk main loop. We initialize sensible dfk_http_t properties instead.
   */
  f->http.dfk = &f->dfk;
  f->http.keepalive_requests = DFK_HTTP_KEEPALIVE_REQUESTS;
  f->http.header_max_size = DFK_HTTP_HEADER_MAX_SIZE;
  f->http.headers_buffer_size = DFK_HTTP_HEADERS_BUFFER_SIZE;
  f->http.headers_buffer_count = DFK_HTTP_HEADERS_BUFFER_COUNT;

  dfk_arena_init(&f->conn_arena, &f->dfk);
  dfk_arena_init(&f->req_arena, &f->dfk);
  dfk__http_request_init(&f->req, &f->http, &f->req_arena,
      &f->conn_arena, &f->sock);
  dfk__sponge_init(&f->reqbuf, &f->dfk);
  f->req._socket_mocked |= 1;
  f->req._socket_mock = &f->reqbuf;
}


static void fixture_teardown(fixture_t* f)
{
  dfk__sponge_free(&f->reqbuf);
  dfk__http_request_free(&f->req);
  dfk_arena_free(&f->conn_arena);
  dfk_arena_free(&f->req_arena);
  dfk_free(&f->dfk);
}


typedef ssize_t(*dfk_read_f)(void*, char*, size_t);
static ssize_t readall(dfk_read_f readfunc, void* readobj, char* buf, size_t size)
{
  ssize_t total = 0;
  ssize_t res = readfunc(readobj, buf, size);
  while (res > 0) {
    total += res;
    buf += res;
    size -= res;
    if (!size) {
      break;
    }
    res = readfunc(readobj, buf, size);
  }
  return size ? res : total;
}


TEST_F(fixture, http_request, trivial)
{
  char request[] = "GET / HTTP/1.0\r\n\r\n";
  dfk__sponge_write(&fixture->reqbuf, request, sizeof(request) - 1);
  EXPECT_OK(dfk__http_request_read_headers(&fixture->req));
  EXPECT(fixture->req.content_length == 0);
  EXPECT(fixture->req.method == DFK_HTTP_GET);
  EXPECT_BUFSTREQ(fixture->req.url, "/");
  EXPECT(fixture->req.major_version == 1);
  EXPECT(fixture->req.minor_version == 0);
}


TEST_F(fixture, http_request, http_version_1_1)
{
  char request[] = "GET / HTTP/1.1\r\n\r\n";
  dfk__sponge_write(&fixture->reqbuf, request, sizeof(request) - 1);
  EXPECT_OK(dfk__http_request_read_headers(&fixture->req));
  EXPECT(fixture->req.major_version == 1);
  EXPECT(fixture->req.minor_version == 1);
}

TEST_F(fixture, http_request, method_post)
{
    char request[] = "POST / HTTP/1.1\r\n\r\n";
    dfk__sponge_write(&fixture->reqbuf, request, sizeof(request) - 1);
    EXPECT_OK(dfk__http_request_read_headers(&fixture->req));
    EXPECT(fixture->req.method == DFK_HTTP_POST);
}


TEST_F(fixture, http_request, method_head)
{
  char request[] = "HEAD / HTTP/1.1\r\n\r\n";
  dfk__sponge_write(&fixture->reqbuf, request, sizeof(request) - 1);
  EXPECT_OK(dfk__http_request_read_headers(&fixture->req));
  EXPECT(fixture->req.method == DFK_HTTP_HEAD);
}


TEST_F(fixture, http_request, method_delete)
{
  char request[] = "DELETE / HTTP/1.1\r\n\r\n";
  dfk__sponge_write(&fixture->reqbuf, request, sizeof(request) - 1);
  EXPECT_OK(dfk__http_request_read_headers(&fixture->req));
  EXPECT(fixture->req.method == DFK_HTTP_DELETE);
}


TEST_F(fixture, http_request, trivial_chunked_body)
{
  char request[] = "POST / HTTP/1.0\r\n"
                   "Transfer-Encoding: chunked\r\n"
                   "\r\n"
                   "5\r\n"
                   "Hello\r\n"
                   "7\r\n"
                   ", world\r\n"
                   "0\r\n";
  dfk__sponge_write(&fixture->reqbuf, request, sizeof(request) - 1);
  EXPECT_OK(dfk__http_request_read_headers(&fixture->req));
  EXPECT(fixture->req.content_length == -1);
  char buf[12] = {0};
  EXPECT(readall((dfk_read_f) dfk_http_request_read, &fixture->req, buf, sizeof(buf)) == 12);
  EXPECT(!strncmp(buf, "Hello, world", sizeof(buf)));
}


TEST_F(fixture, http_request, url_with_arguments)
{
  char request[] = "GET /foo/bar?opt1=value1&option2=value%202 HTTP/1.1\r\n"
                   "\r\n";
  dfk__sponge_write(&fixture->reqbuf, request, sizeof(request) - 1);
  EXPECT_OK(dfk__http_request_read_headers(&fixture->req));
  EXPECT_BUFSTREQ(fixture->req.url, "/foo/bar?opt1=value1&option2=value%202");
  EXPECT_BUFSTREQ(fixture->req.path, "/foo/bar");
  EXPECT(dfk_strmap_size(&fixture->req.arguments) == 2);
  EXPECT_BUFSTREQ(dfk_strmap_get(&fixture->req.arguments, "opt1", 4), "value1");
  EXPECT_BUFSTREQ(dfk_strmap_get(&fixture->req.arguments, "option2", 7), "value 2");
}


TEST_F(fixture, http_request, host)
{
  char request[] = "GET / HTTP/1.0\r\n"
                   "Host: www.example.com\r\n"
                   "\r\n";
  dfk__sponge_write(&fixture->reqbuf, request, sizeof(request) - 1);
  EXPECT_OK(dfk__http_request_read_headers(&fixture->req));
  EXPECT_BUFSTREQ(fixture->req.host, "www.example.com");
}


TEST_F(fixture, http_request, url_with_fragment)
{
  char request[] = "GET /foo#fragment1 HTTP/1.1\r\n"
                   "\r\n";
  dfk__sponge_write(&fixture->reqbuf, request, sizeof(request) - 1);
  EXPECT_OK(dfk__http_request_read_headers(&fixture->req));
  EXPECT(fixture->req.content_length == 0);
  EXPECT(fixture->req.method == DFK_HTTP_GET);
  EXPECT_BUFSTREQ(fixture->req.fragment, "fragment1");
}


#endif /* DFK_MOCKS */

