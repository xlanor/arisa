#include <arisa/motion/transaction_stack.hpp>

#include <atomic>
#include <thread>
#include <utility>

namespace arisa::motion {

struct TransactionStack::Slot {
  using Owner = decltype(std::this_thread::get_id());

  std::atomic<Owner> owner{};
  const Scope* top{nullptr};
};

auto TransactionStack::slots() noexcept -> std::array<Slot, capacity>& {
  static std::array<Slot, capacity> storage{};
  return storage;
}

auto TransactionStack::find() noexcept -> std::optional<std::size_t> {
  const Slot::Owner self = std::this_thread::get_id();
  std::array<Slot, capacity>& table = slots();
  for (std::size_t index = 0; index < capacity; ++index) {
    if (table[index].owner.load(std::memory_order_acquire) == self) {
      return index;
    }
  }
  return std::nullopt;
}

auto TransactionStack::claim() noexcept -> std::optional<std::size_t> {
  const Slot::Owner self = std::this_thread::get_id();
  std::array<Slot, capacity>& table = slots();
  for (std::size_t index = 0; index < capacity; ++index) {
    Slot::Owner vacant{};
    if (table[index].owner.compare_exchange_strong(vacant, self,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
      return index;
    }
  }
  return std::nullopt;
}

TransactionStack::Scope::Scope(Transaction transaction) noexcept
    : transaction_(std::move(transaction)) {
  slot_ = find();
  if (!slot_.has_value()) {
    slot_ = claim();
  }
  if (slot_.has_value()) {
    Slot& entry = slots()[*slot_];
    previous_ = entry.top;
    entry.top = this;
  }
}

TransactionStack::Scope::~Scope() {
  if (!slot_.has_value()) {
    return;
  }
  Slot& entry = slots()[*slot_];
  entry.top = previous_;
  if (previous_ == nullptr) {
    entry.owner.store(Slot::Owner{}, std::memory_order_release);
  }
}

auto TransactionStack::current() noexcept
    -> std::optional<std::reference_wrapper<const Transaction>> {
  const auto slot = find();
  if (!slot.has_value()) {
    return std::nullopt;
  }
  const Scope* top = slots()[*slot].top;
  if (top == nullptr) {
    return std::nullopt;
  }
  return std::cref(top->transaction());
}

}
