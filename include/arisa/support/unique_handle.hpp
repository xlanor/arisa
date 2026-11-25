#pragma once

#include <concepts>
#include <utility>

namespace arisa::support {

template <class Traits>
concept HandleTraits = requires(typename Traits::handle_type& handle,
                                const typename Traits::handle_type& view) {
  { Traits::empty() } noexcept -> std::same_as<typename Traits::handle_type>;
  { Traits::is_valid(view) } noexcept -> std::same_as<bool>;
  { Traits::release(handle) } noexcept;
};

template <HandleTraits Traits>
class UniqueHandle {
 public:
  using handle_type = typename Traits::handle_type;

  UniqueHandle() noexcept : handle_(Traits::empty()) {}

  explicit UniqueHandle(handle_type handle) noexcept
      : handle_(std::move(handle)) {}

  UniqueHandle(const UniqueHandle&) = delete;
  auto operator=(const UniqueHandle&) -> UniqueHandle& = delete;

  UniqueHandle(UniqueHandle&& other) noexcept
      : handle_(std::move(other.handle_)) {
    other.handle_ = Traits::empty();
  }

  auto operator=(UniqueHandle&& other) noexcept -> UniqueHandle& {
    if (this != &other) {
      reset();
      handle_ = std::move(other.handle_);
      other.handle_ = Traits::empty();
    }
    return *this;
  }

  ~UniqueHandle() { reset(); }

  auto get() const noexcept -> const handle_type& { return handle_; }

  auto valid() const noexcept -> bool { return Traits::is_valid(handle_); }

  explicit operator bool() const noexcept { return valid(); }

  void reset() noexcept {
    if (Traits::is_valid(handle_)) {
      Traits::release(handle_);
    }
    handle_ = Traits::empty();
  }

 private:
  handle_type handle_;
};

}
