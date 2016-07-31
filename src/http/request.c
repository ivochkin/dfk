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
#include <dfk/http/request.h>
#include <dfk/internal.h>


int dfk_http_request_init(dfk_http_request_t* req, dfk_t* dfk,
                          dfk_arena_t* request_arena,
                          dfk_arena_t* connection_arena,
                          dfk_tcp_socket_t* sock)
{
  assert(req);
  assert(request_arena);
  assert(connection_arena);
  assert(sock);
  req->_request_arena = request_arena;
  req->_connection_arena = connection_arena;
  req->_sock = sock;
  DFK_CALL(dfk, dfk_strmap_init(&req->headers));
  DFK_CALL(dfk, dfk_strmap_init(&req->arguments));
  req->_bodypart = (dfk_buf_t) {NULL, 0};
  req->_body_bytes_nread = 0;
  req->_headers_done = 0;
#if DFK_MOCKS
  req->_sock_mocked = 0;
  req->_sock_mock = 0;
#endif
  req->dfk = dfk;
  req->url = (dfk_buf_t) {NULL, 0};
  req->user_agent = (dfk_buf_t) {NULL, 0};
  req->host = (dfk_buf_t) {NULL, 0};
  req->content_type = (dfk_buf_t) {NULL, 0};
  req->content_length = 0;
  return dfk_err_ok;
}


int dfk_http_request_free(dfk_http_request_t* req)
{
  DFK_CALL(req->dfk, dfk_strmap_free(&req->arguments));
  DFK_CALL(req->dfk, dfk_strmap_free(&req->headers));
  return dfk_err_ok;
}


ssize_t dfk_http_request_read(dfk_http_request_t* req, char* buf, size_t size)
{
  if (!req || (!buf && size)) {
    return dfk_err_badarg;
  }

  DFK_DBG(req->dfk, "{%p} upto %llu bytes", (void*) req, (unsigned long long) size);

  if (!req->content_length) {
    return dfk_err_eof;
  }

  if (req->_body_bytes_nread < req->_bodypart.size) {
    DFK_DBG(req->dfk, "{%p} body part of size %llu is cached, copy",
            (void*) req, (unsigned long long) req->_bodypart.size);
    size_t tocopy = DFK_MIN(size, req->_bodypart.size - req->_body_bytes_nread);
    memcpy(buf, req->_bodypart.data, tocopy);
    req->_body_bytes_nread += tocopy;
    return tocopy;
  }
  size_t toread = DFK_MIN(size, req->content_length - req->_body_bytes_nread);
#if DFK_MOCKS
  if (req->_sock_mocked) {
    return dfk_sponge_read(req->_sock_mock, buf, toread);
  } else {
    return dfk_tcp_socket_read(req->_sock, buf, toread);
  }
#else
  return dfk_tcp_socket_read(req->_sock, buf, toread);
#endif
}


ssize_t dfk_http_request_readv(dfk_http_request_t* req, dfk_iovec_t* iov, size_t niov)
{
  if (!req || (!iov && niov)) {
    return dfk_err_badarg;
  }
  DFK_DBG(req->dfk, "{%p} into %llu blocks", (void*) req, (unsigned long long) niov);
  return dfk_http_request_read(req, iov[0].data, iov[0].size);
}

