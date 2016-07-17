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
#include <string.h>
#include <dfk/config.h>
#include <dfk/internal.h>
#include <dfk/internal/sponge.h>


#ifdef NDEBUG
#define DFK_SPONGE_CHECK_INVARIANTS(sponge) DFK_UNUSED(sponge)
#else

static void dfk__sponge_check_invariants(dfk_sponge_t* sponge)
{
  assert(sponge);
  assert(sponge->dfk);
  if (!sponge->base) {
    assert(!sponge->size);
    assert(!sponge->cur);
    assert(!sponge->capacity);
  } else {
    assert(sponge->cur);
    assert(sponge->base <= sponge->cur);
    assert(sponge->cur <= sponge->base + sponge->size);
    assert(sponge->base + sponge->size <= sponge->base + sponge->capacity);
  }
}

#define DFK_SPONGE_CHECK_INVARIANTS(sponge) dfk__sponge_check_invariants((sponge))
#endif /* NDEBUG */


void dfk_sponge_init(dfk_sponge_t* sponge, dfk_t* dfk)
{
  assert(sponge);
  assert(dfk);
  memset(sponge, 0, sizeof(*sponge));
  sponge->dfk = dfk;
}


void dfk_sponge_free(dfk_sponge_t* sponge)
{
  assert(sponge);
  DFK_FREE(sponge->dfk, sponge->base);
}


int dfk_sponge_write(dfk_sponge_t* sponge, char* buf, size_t nbytes)
{
  assert(sponge);
  assert(buf || !nbytes);
  DFK_SPONGE_CHECK_INVARIANTS(sponge);
  if (!sponge->capacity) {
    /* Empty sponge - perform initial allocation */
    size_t toalloc = DFK_MAX(nbytes, DFK_SPONGE_INITIAL_SIZE);
    sponge->base = DFK_MALLOC(sponge->dfk, toalloc);
    if (!sponge->base) {
      return dfk_err_nomem;
    }
    sponge->cur = sponge->base;
    sponge->size = nbytes;
    sponge->capacity = toalloc;
    memcpy(sponge->base, buf, nbytes);
  } else if (nbytes <= sponge->capacity - sponge->size) {
    /* Lucky - use available space, no relocations needed */
    memcpy(sponge->base + sponge->size, buf, nbytes);
    sponge->size += nbytes;
  } else {
    /* Need reallocation */
    size_t nused = sponge->base + sponge->size - sponge->cur;
    size_t newsize = sponge->capacity + DFK_MAX(nused + nbytes, sponge->capacity);
    char* newbase = DFK_MALLOC(sponge->dfk, newsize);
    if (!newbase) {
      return dfk_err_nomem;
    }
    memcpy(newbase, sponge->cur, nused);
    memcpy(newbase + nused, buf, nbytes);
    DFK_FREE(sponge->dfk, sponge->base);
    sponge->base = newbase;
    sponge->cur = newbase;
    sponge->capacity = newsize;
    sponge->size = nused + nbytes;
  }
  DFK_SPONGE_CHECK_INVARIANTS(sponge);
  return dfk_err_ok;
}


int dfk_sponge_writev(dfk_sponge_t* sponge, dfk_iovec_t* iov, size_t niov)
{
  assert(sponge);
  assert(iov || !niov);
  DFK_SPONGE_CHECK_INVARIANTS(sponge);
  for (size_t i = 0; i < niov; ++i) {
    DFK_CALL(sponge->dfk, dfk_sponge_write(sponge, iov[i].data, iov[i].size));
  }
  DFK_SPONGE_CHECK_INVARIANTS(sponge);
  return dfk_err_ok;
}


ssize_t dfk_sponge_read(dfk_sponge_t* sponge, char* buf, size_t nbytes)
{
  assert(sponge);
  assert(buf || !nbytes);
  DFK_SPONGE_CHECK_INVARIANTS(sponge);
  if (!sponge->size) {
    return 0;
  }
  size_t toread = DFK_MIN(nbytes, (size_t) (sponge->base + sponge->size - sponge->cur));
  memcpy(buf, sponge->cur, toread);
  sponge->cur += toread;
  DFK_SPONGE_CHECK_INVARIANTS(sponge);
  return toread;
}


ssize_t dfk_sponge_readv(dfk_sponge_t* sponge, dfk_iovec_t* iov, size_t niov)
{
  assert(sponge);
  assert(iov || !niov);
  DFK_SPONGE_CHECK_INVARIANTS(sponge);
  ssize_t totalnread = 0;
  for (size_t i = 0; i < niov; ++i) {
    ssize_t nread = dfk_sponge_read(sponge, iov[i].data, iov[i].size);
    totalnread += nread;
    if (!nread) {
      break;
    }
  }
  DFK_SPONGE_CHECK_INVARIANTS(sponge);
  return totalnread;
}
