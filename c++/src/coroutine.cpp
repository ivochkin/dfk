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


#include <dfk/coroutine.hpp>
#include "common.hpp"

namespace dfk {

Coroutine::Coroutine(dfk_coro_t* coro)
  : Wrapper(coro)
{
}

void Coroutine::setName(const char* name)
{
  DFK_ENSURE_OK(context(), dfk_coro_name(nativeHandle(), "%s", name));
}

Context* Coroutine::context()
{
  return static_cast<Context*>(nativeHandle()->dfk->user.data);
}

void Coroutine::yield(Coroutine* to)
{
  DFK_ENSURE_OK(context(), dfk_yield(this->nativeHandle(), to->nativeHandle()));
}

} // namespace dfk