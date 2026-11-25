#pragma once

#include <arisa/motion/transaction.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <optional>

namespace arisa::motion {

class TransactionStack {
 public:
  static constexpr std::size_t capacity = 32;

  class Scope {
   public:
    explicit Scope(Transaction transaction) noexcept;

    Scope(const Scope&) = delete;
    auto operator=(const Scope&) -> Scope& = delete;
    Scope(Scope&&) = delete;
    auto operator=(Scope&&) -> Scope& = delete;
    ~Scope();

    auto transaction() const noexcept -> const Transaction& {
      return transaction_;
    }

   private:
    Transaction transaction_;
    const Scope* previous_{nullptr};
    std::optional<std::size_t> slot_;
  };

  static auto current() noexcept
      -> std::optional<std::reference_wrapper<const Transaction>>;

 private:
  struct Slot;

  static auto slots() noexcept -> std::array<Slot, capacity>&;

  static auto find() noexcept -> std::optional<std::size_t>;

  static auto claim() noexcept -> std::optional<std::size_t>;
};

}
