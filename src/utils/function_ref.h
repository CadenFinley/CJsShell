/*
  function_ref.h

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace cjsh {

template <typename>
class FunctionRef;

template <typename R, typename... Args>
class FunctionRef<R(Args...)> {
   public:
    FunctionRef() = delete;

    FunctionRef(const FunctionRef&) = default;
    FunctionRef& operator=(const FunctionRef&) = default;

    template <typename Callable,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<Callable>, FunctionRef>>>
    FunctionRef(Callable&& callable) noexcept
        : object_(const_cast<void*>(static_cast<const void*>(std::addressof(callable)))),
          callback_([](void* object, Args... args) -> R {
              using Callback = std::remove_cv_t<std::remove_reference_t<Callable>>;
              return (*static_cast<const Callback*>(object))(std::forward<Args>(args)...);
          }) {
    }

    R operator()(Args... args) const {
        return callback_(object_, std::forward<Args>(args)...);
    }

   private:
    void* object_;
    R (*callback_)(void*, Args...);
};

}  // namespace cjsh
