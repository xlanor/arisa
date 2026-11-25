#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/ut_type.hpp>
#include <arisa/provider/clipboard/provider.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace arisa::composition {

class Pasteboard {
 public:
  static const EnvironmentKey<std::optional<Pasteboard>> general;

  Pasteboard();

  explicit Pasteboard(provider::ClipboardProvider& clipboard);

  void set_items(std::vector<TransferItem> items) const;

  auto items() const -> std::vector<TransferItem>;

  auto contains(const UTType& type) const -> bool;

  auto change_count() const noexcept -> std::uint64_t;

  friend auto operator==(const Pasteboard& left, const Pasteboard& right)
      -> bool {
    return left.state_ == right.state_;
  }

 private:
  struct State;

  std::shared_ptr<State> state_;
};

}
