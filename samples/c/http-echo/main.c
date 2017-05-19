/**
 * @copyright
 * Copyright (c) 2016-2017 Stanislav Ivochkin
 * Licensed under the MIT License (see LICENSE)
 */

#include <stdlib.h>
#include <stdio.h>
#include <dfk.h>


typedef struct args_t {
  int argc;
  char** argv;
} args_t;


#define CALL_DFK_API(c) if ((c) != dfk_err_ok) { return -1; }


static int echo(dfk_userdata_t ud, dfk_http_t* http, dfk_http_request_t* req, dfk_http_response_t* resp)
{
  dfk_strmap_it it;
  (void) ud;
  (void) http;
  CALL_DFK_API(dfk_strmap_begin(&req->headers, &it));
  while (dfk_strmap_it_valid(&it) == dfk_err_ok) {
    dfk_buf_t name = it.item->key;
    dfk_buf_t value = it.item->value;
    CALL_DFK_API(dfk_http_response_bset(resp, name, value));
    CALL_DFK_API(dfk_strmap_it_next(&it));
  }
  if (req->content_length > 0) {
    char buf[4096] = {0};
    ssize_t nread = 0;
    while (nread >= 0) {
      nread = dfk_http_request_read(req, buf, sizeof(buf));
      if (nread < 0) {
        return -1;
      }
      ssize_t nwritten = dfk_http_response_write(resp, buf, nread);
      if (nwritten < 0) {
        return -1;
      }
    }
  }

  resp->status = DFK_HTTP_OK;
  return dfk_err_ok;;
}


static void dfk_main(dfk_coro_t* coro, void* p)
{
  dfk_http_t srv;
  dfk_userdata_t ud = {NULL};
  args_t* args = (args_t*) p;
  if (dfk_http_init(&srv, coro->dfk) != dfk_err_ok) {
    return;
  }
  if (dfk_http_serve(&srv, args->argv[1], atoi(args->argv[2]), echo, ud) != dfk_err_ok) {
    return;
  }
  dfk_http_free(&srv);
}


int main(int argc, char** argv)
{
  dfk_t dfk;
  args_t args;
  if (argc != 3) {
    fprintf(stderr, "usage: %s <ip> <port>\n", argv[0]);
    return 1;
  }
  args.argc = argc;
  args.argv = argv;
  dfk_init(&dfk);
  (void) dfk_run(&dfk, dfk_main, &args, 0);
  CALL_DFK_API(dfk_work(&dfk));
  return dfk_free(&dfk);
}
