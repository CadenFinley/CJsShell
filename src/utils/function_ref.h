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
