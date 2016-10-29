/**
 * @file dfk/context.hpp
 *
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

#pragma once
#include <cstddef>
#include <dfk/core.h>
#include <dfk/core.hpp>
#include <dfk/wrapper.hpp>
#include <dfk/coroutine.hpp>

namespace dfk {

class Context : public Wrapper<dfk_t, dfk_sizeof>
{
public:
  Context();
  ~Context();

private:
  Context(const Context&);
  Context& operator=(const Context&);

public:
  virtual void* malloc(std::size_t nbytes);
  virtual void free(void* p);
  virtual void* realloc(void* p, std::size_t nbytes);
  virtual void log(LogLevel level, const char* msg);

  void setDefaultStackSize(std::size_t stackSize);
  std::size_t defaultStackSize() const;
  void work();
  void stop();
  void sleep(uint64_t msec);
  /// @todo Refactor it. Need template method similar to boost::bind
  Coroutine run(void (*func)(Coroutine, void*), void* arg, size_t argSize);
};

} // namespace dfk