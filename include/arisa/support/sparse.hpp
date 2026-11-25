#pragma once

#include <arisa/support/sparse_recycling.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <memory>
#include <mutex>
#include <span>
#include <utility>

namespace arisa::support {

template <class T>
class Sparse {
 public:
  Sparse() noexcept = default;

  Sparse(const Sparse& other)
    requires std::copy_constructible<T>
      : value_(other.value_ ? make(*other.value_) : nullptr) {}

  Sparse(Sparse&&) noexcept = default;

  auto operator=(const Sparse& other) -> Sparse&
    requires std::copy_constructible<T>
  {
    if (this != &other) {
      value_ = other.value_ ? make(*other.value_) : nullptr;
    }
    return *this;
  }

  auto operator=(Sparse&&) noexcept -> Sparse& = default;

  ~Sparse() = default;

  auto operator*() const noexcept -> const T& {
    return value_ ? *value_ : fallback();
  }

  auto operator->() const noexcept -> const T* { return &**this; }

  auto edit() -> T& {
    if (!value_) {
      value_ = make();
    }
    return *value_;
  }

  auto take() -> T {
    if (!value_) {
      return T{};
    }
    T taken = std::move(*value_);
    value_.reset();
    return taken;
  }

  template <class Member>
  auto extract(Member T::* member) -> Member {
    if (!value_) {
      return Member{};
    }
    return std::move((*value_).*member);
  }

  auto engaged() const noexcept -> bool { return value_ != nullptr; }

  void reset() noexcept { value_.reset(); }

  void clear() {
    if (value_) {
      *value_ = T{};
    }
  }

  friend auto operator==(const Sparse& left, const Sparse& right) -> bool
    requires std::equality_comparable<T>
  {
    return left.value_ == right.value_ || *left == *right;
  }

 private:
  class Recycler {
   public:
    constexpr Recycler() noexcept = default;

    Recycler(const Recycler&) = delete;
    auto operator=(const Recycler&) -> Recycler& = delete;
    Recycler(Recycler&&) = delete;
    auto operator=(Recycler&&) -> Recycler& = delete;

    ~Recycler() {
      std::allocator<T> allocator;
      for (T* spare : std::span{spares_}.first(count_)) {
        allocator.deallocate(spare, 1);
      }
    }

    auto acquire() -> T* {
      if (SparseRecycling::active()) {
        const std::lock_guard held{mutex_};
        if (count_ > 0) {
          --count_;
          return std::exchange(spares_[count_], nullptr);
        }
      }
      return std::allocator<T>{}.allocate(1);
    }

    void release(T* storage) noexcept {
      if (SparseRecycling::active()) {
        const std::lock_guard held{mutex_};
        if (count_ < capacity) {
          spares_[count_] = storage;
          ++count_;
          return;
        }
      }
      std::allocator<T>{}.deallocate(storage, 1);
    }

   private:
    static constexpr std::size_t capacity = 64;

    std::mutex mutex_;
    std::array<T*, capacity> spares_{};
    std::size_t count_{0};
  };

  class Recycle {
   public:
    void operator()(T* block) const noexcept {
      std::destroy_at(block);
      recycler_.release(block);
    }
  };

  using Block = std::unique_ptr<T, Recycle>;

  template <class... Arguments>
  static auto make(Arguments&&... arguments) -> Block {
    return Block{std::construct_at(recycler_.acquire(),
                                   std::forward<Arguments>(arguments)...)};
  }

  static auto fallback() noexcept -> const T& {
    static const T empty{};
    return empty;
  }

  static constinit inline Recycler recycler_{};

  Block value_;
};

}
