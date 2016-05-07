/**
 * @copyright
 * Copyright (c) 2015, 2016, Stanislav Ivochkin. All Rights Reserved.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dfk.h>
#include <dfk/internal.h>


static void dfk_default_log(void* ud, int channel, const char* msg)
{
  char strchannel[5] = {0};
  (void) ud;
  switch(channel) {
    case dfk_log_error: memcpy(strchannel, "error", 5); break;
    case dfk_log_warning: memcpy(strchannel, "warn_", 5); break;
    case dfk_log_info: memcpy(strchannel, "info_", 5); break;
    case dfk_log_debug: memcpy(strchannel, "debug", 5); break;
    default: snprintf(strchannel, sizeof(strchannel), "%5d", channel);
  }
  /* At most 512 bytes will printed at once */
  printf("[%.5s] %.503s\n", strchannel, msg);
  fflush(stdout);
}


static void* dfk_default_malloc(void* dfk, size_t size)
{
  void* ret;
  ret = malloc(size);
  DFK_DEBUG((dfk_t*) dfk, "%llu bytes requested = %p",
      (unsigned long long) size, ret);
  return ret;
}


static void dfk_default_free(void* dfk, void* p)
{
  DFK_DEBUG((dfk_t*) dfk, "release memory %p", (void*) p);
  free(p);
}


static void* dfk_default_realloc(void* dfk, void* p, size_t size)
{
  void* ret;
  ret = realloc(p, size);
  DFK_DEBUG((dfk_t*) dfk, "resize %p to %llu bytes requested = %p",
      p, (unsigned long long) size, ret);
  return ret;
}


int dfk_init(dfk_t* dfk)
{
  if (!dfk) {
    return dfk_err_badarg;
  }
  dfk->_.exechead = NULL;
  dfk->_.termhead = NULL;
  dfk->_.scheduler = NULL;
  dfk->malloc = dfk_default_malloc;
  dfk->free = dfk_default_free;
  dfk->realloc = dfk_default_realloc;
#ifdef DFK_DEBUG_ENABLED
  dfk->log = dfk_default_log;
#else
  DFK_UNUSED(dfk_default_log);
  dfk->log = NULL;
#endif
  dfk->default_stack_size = DFK_STACK_SIZE;

  dfk->sys_errno = 0;
  dfk->dfk_errno = 0;

  return dfk_err_ok;
}


int dfk_free(dfk_t* dfk)
{
  if (!dfk) {
    return dfk_err_badarg;
  }
  return dfk_err_ok;
}


int dfk_run(dfk_t* dfk, void (*ep)(dfk_t*, void*), void* arg)
{
  if (!dfk || !ep) {
    return dfk_err_badarg;
  }
  dfk_coro_t* coro = dfk_coro_run(dfk, ep, arg);
  if (!coro) {
    return dfk->dfk_errno;
  }
  return dfk_err_ok;
}


static void dfk_scheduler(dfk_t* dfk, void* p)
{
  DFK_UNUSED(p);
  while (1) {
    if (!dfk->_.termhead && !dfk->_.exechead) {
      break;
    }
    {
      /* cleanup terminated coroutines */
      dfk_coro_t* i = dfk->_.termhead;
      while (i) {
        dfk_coro_free(i);
        i = i->_.next;
      }
      dfk->_.termhead = NULL;
    }
    if (dfk->_.exechead) {
      dfk_coro_t* coro = dfk->_.exechead;
      dfk->_.exechead = coro->_.next;
      DFK_DEBUG(dfk, "schedule to run coroutine {%p}", (void*) coro);
      dfk_coro_yield(dfk->_.scheduler, coro);
    }
  }
  dfk_coro_yield(dfk->_.scheduler, NULL);
  DFK_INFO(dfk, "no pending coroutines left in execution queue, jobs done");
}


int dfk_work(dfk_t* dfk)
{
  int ret;
  if (!dfk) {
    return dfk_err_badarg;
  }
  dfk->_.scheduler = dfk_coro_run(dfk, dfk_scheduler, NULL);
  if (!dfk->_.scheduler) {
    return dfk->dfk_errno;
  }
  if ((ret = dfk_coro_yield(NULL, dfk->_.scheduler)) != dfk_err_ok) {
    return ret;
  }
  return dfk_coro_free(dfk->_.scheduler);
}


const char* dfk_strerr(dfk_t* dfk, int err)
{
  switch(err) {
    case dfk_err_ok: {
      return "No error";
    }
    case dfk_err_nomem: {
      return "Memory allocation function returned NULL";
    }
    case dfk_err_notfound: {
      return "Object not found";
    }
    case dfk_err_badarg: {
      return "Bad argument";
    }
    case dfk_err_sys: {
      return dfk ? strerror(dfk->sys_errno) :
        "System error, dfk_t object is NULL, can not access sys_errno";
    }
    case dfk_err_inprog: {
      return "The operation is already in progress";
    }
    default: {
      return "Unknown error";
    }
  }
  return "Unknown error";
}

