/**
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

#include <assert.h>
#include <dfk/http/response.h>
#include <dfk/internal.h>


void dfk__http_response_init(dfk_http_response_t* resp, dfk_t* dfk,
                             dfk_arena_t* request_arena,
                             dfk_arena_t* connection_arena,
                             dfk_tcp_socket_t* sock)
{
  assert(resp);
  assert(request_arena);
  assert(connection_arena);
  assert(sock);

  resp->_request_arena = request_arena;
  resp->_connection_arena = connection_arena;
  resp->_sock = sock;
  dfk_avltree_init(&resp->_headers, dfk__http_headers_cmp);
#if DFK_MOCKS
  resp->_sock_mocked = 0;
  resp->_sock_mock = 0;
#endif
  resp->_headers_flushed = 0;

  resp->dfk = dfk;
  resp->major_version = 1;
  resp->minor_version = 0;
  resp->code = DFK_HTTP_OK;
  resp->content_length = (size_t) -1;
  resp->chunked = 0;
}


void dfk__http_response_free(dfk_http_response_t* resp)
{
  assert(resp);
  dfk_avltree_free(&resp->_headers);
}


int dfk_http_set(dfk_http_response_t* resp, const char* name, size_t namesize, const char* value, size_t valuesize)
{
  if (!resp || (!name && namesize) || (!value && valuesize)) {
    return dfk_err_badarg;
  }
  DFK_DBG(resp->dfk, "{%p} %.*s: %.*s", (void*) resp, (int) namesize, name,
          (int) valuesize, value);
  dfk_http_header_t* header = dfk_arena_alloc(resp->_request_arena,
                                              sizeof(dfk_http_header_t));
  if (!header) {
    return dfk_err_nomem;
  }
  dfk__http_header_init(header);
  header->name = (dfk_buf_t) {(char*) name, namesize};
  header->value = (dfk_buf_t) {(char*) value, valuesize};
  dfk_avltree_insert(&resp->_headers, (dfk_avltree_hook_t*) header);
  return dfk_err_ok;
}


int dfk_http_set_copy(dfk_http_response_t* resp, const char* name,
                      size_t namesize, const char* value, size_t valuesize)
{
  if (!resp || (!name && namesize) || (!value && valuesize)) {
    return dfk_err_badarg;
  }
  DFK_DBG(resp->dfk, "{%p} %.*s: %.*s", (void*) resp, (int) namesize, name, (int) valuesize, value);
  void* namecopy = dfk_arena_alloc_copy(resp->_request_arena, name, namesize);
  if (!namecopy) {
    return dfk_err_nomem;
  }
  void* valuecopy = dfk_arena_alloc_copy(resp->_request_arena, value, valuesize);
  if (!valuecopy) {
    return dfk_err_nomem;
  }
  return dfk_http_set(resp, namecopy, namesize, valuecopy, valuesize);
}


int dfk_http_set_copy_name(dfk_http_response_t* resp, const char* name,
                           size_t namesize, const char* value,
                           size_t valuesize)
{
  if (!resp || (!name && namesize) || (!value && valuesize)) {
    return dfk_err_badarg;
  }
  DFK_DBG(resp->dfk, "{%p} %.*s: %.*s", (void*) resp, (int) namesize, name, (int) valuesize, value);
  void* namecopy = dfk_arena_alloc_copy(resp->_request_arena, name, namesize);
  if (!namecopy) {
    return dfk_err_nomem;
  }
  return dfk_http_set(resp, namecopy, namesize, value, valuesize);
}


int dfk_http_set_copy_value(dfk_http_response_t* resp, const char* name,
                            size_t namesize, const char* value,
                            size_t valuesize)
{
  if (!resp || (!name && namesize) || (!value && valuesize)) {
    return dfk_err_badarg;
  }
  DFK_DBG(resp->dfk, "{%p} %.*s: %.*s", (void*) resp, (int) namesize, name, (int) valuesize, value);
  void* valuecopy = dfk_arena_alloc_copy(resp->_request_arena, value, valuesize);
  if (!valuecopy) {
    return dfk_err_nomem;
  }
  return dfk_http_set(resp, name, namesize, valuecopy, valuesize);
}


int dfk__http_response_flush_headers(dfk_http_response_t* resp)
{
  assert(resp);
  if (resp->_headers_flushed) {
    return dfk_err_ok;
  }

  size_t totalheaders = dfk_avltree_size(&resp->_headers);
  size_t niov = 4 * totalheaders + 2;
  dfk_iovec_t* iov = dfk_arena_alloc(resp->_request_arena, niov * sizeof(dfk_iovec_t));
  if (!iov) {
    resp->code = DFK_HTTP_INTERNAL_SERVER_ERROR;
  }

  char sbuf[128] = {0};
  int ssize = snprintf(sbuf, sizeof(sbuf), "HTTP/%d.%d %3d %s\r\n",
                       resp->major_version, resp->minor_version, resp->code,
                       dfk__http_reason_phrase(resp->code));
  if (!iov) {
#if DFK_MOCKS
    if (resp->_sock_mocked) {
      dfk_sponge_write(resp->_sock_mock, sbuf, ssize);
    } else {
      dfk_tcp_socket_write(resp->_sock, sbuf, ssize);
    }
#else
    dfk_tcp_socket_write(resp->_sock, sbuf, ssize);
#endif
  } else {
    size_t i = 0;
    iov[i++] = (dfk_iovec_t) {sbuf, ssize};
    dfk_avltree_it_t it;
    dfk_avltree_it_init(&resp->_headers, &it);
    while (dfk_avltree_it_valid(&it) && i < niov) {
      dfk_http_header_t* h = (dfk_http_header_t*) it.value;
      iov[i++] = h->name;
      iov[i++] = (dfk_iovec_t) {": ", 2};
      iov[i++] = h->value;
      iov[i++] = (dfk_iovec_t) {"\r\n", 2};
      dfk_avltree_it_next(&it);
    }
    iov[i++] = (dfk_iovec_t) {"\r\n", 2};
#if DFK_MOCKS
    if (resp->_sock_mocked) {
      dfk_sponge_writev(resp->_sock_mock, iov, niov);
    } else {
      dfk_tcp_socket_writev(resp->_sock, iov, niov);
    }
#else
    dfk_tcp_socket_writev(resp->_sock, iov, niov);
#endif
  }
  resp->_headers_flushed |= 1;
  return dfk_err_ok;
}


int dfk__http_response_flush(dfk_http_response_t* resp)
{
  assert(resp);
  return dfk__http_response_flush_headers(resp);
}


dfk_buf_t dfk_http_response_get(dfk_http_response_t* resp,
                                const char* name, size_t namesize)
{
  if (!resp) {
    return (dfk_buf_t) {NULL, 0};
  }
  if (!name && namesize) {
    resp->dfk->dfk_errno = dfk_err_badarg;
    return (dfk_buf_t) {NULL, 0};
  }
  return dfk__http_headers_get(&resp->_headers, name, namesize);
}


int dfk_http_response_headers_begin(dfk_http_response_t* resp,
                                    dfk_http_header_it* it)
{
  if (!resp || !it) {
    return dfk_err_badarg;
  }
  return dfk__http_headers_begin(&resp->_headers, it);
}


ssize_t dfk_http_write(dfk_http_response_t* resp, char* buf, size_t nbytes)
{
  if (!resp) {
    return -1;
  }
  if (!buf && nbytes) {
    resp->dfk->dfk_errno = dfk_err_badarg;
    return -1;
  }
  dfk_iovec_t iov = {buf, nbytes};
  return dfk_http_writev(resp, &iov, 1);
}


ssize_t dfk_http_writev(dfk_http_response_t* resp, dfk_iovec_t* iov, size_t niov)
{
  if (!resp) {
    return -1;
  }
  if (!iov && niov) {
    resp->dfk->dfk_errno = dfk_err_badarg;
    return -1;
  }
  dfk__http_response_flush_headers(resp);
#if DFK_MOCKS
  if (resp->_sock_mocked) {
    return dfk_sponge_writev(resp->_sock_mock, iov, niov);
  } else {
    return dfk_tcp_socket_writev(resp->_sock, iov, niov);
  }
#else
  return dfk_tcp_socket_writev(resp->_sock, iov, niov);
#endif
}
