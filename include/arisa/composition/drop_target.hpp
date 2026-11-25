#pragma once

#include <arisa/composition/drop_delegate.hpp>
#include <arisa/composition/drop_info.hpp>
#include <arisa/composition/drop_session.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/ut_type.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::composition {

class DropTarget {
 public:
  using Perform =
      std::move_only_function<bool(std::span<const TransferItem>, Point)>;
  using Received = std::move_only_function<void(std::span<const TransferItem>,
                                                const DropSession&)>;
  using Targeted = std::move_only_function<void(bool)>;

  DropTarget(std::vector<UTType> accepted, Perform perform,
             Targeted targeted) noexcept
      : accepted_(std::move(accepted)),
        action_(std::move(perform)),
        targeted_(std::move(targeted)) {}

  DropTarget(std::vector<UTType> accepted, Received received,
             bool enabled) noexcept
      : accepted_(std::move(accepted)),
        action_(std::move(received)),
        enabled_(enabled) {}

  DropTarget(std::vector<UTType> accepted,
             std::shared_ptr<DropDelegate> delegate) noexcept
      : accepted_(std::move(accepted)), delegate_(std::move(delegate)) {}

  auto accepts(const TransferItem& item) const -> bool {
    return enabled_ && std::ranges::contains(accepted_, item.type());
  }

  auto accepts(std::span<const TransferItem> items, Point location) const
      -> bool {
    if (!enabled_ ||
        !std::ranges::any_of(items, [this](const TransferItem& item) {
          return accepts(item);
        })) {
      return false;
    }
    if (delegate_ != nullptr) {
      return delegate_->validate_drop(DropInfo{location, items});
    }
    return true;
  }

  auto accepted() const noexcept -> std::span<const UTType> {
    return accepted_;
  }

  auto delegate() const noexcept -> const std::shared_ptr<DropDelegate>& {
    return delegate_;
  }

  auto perform(std::span<const TransferItem> items, Point location) -> bool {
    if (delegate_ != nullptr) {
      return delegate_->perform_drop(DropInfo{location, items});
    }
    if (auto* action = std::get_if<Perform>(&action_); action != nullptr) {
      return *action ? (*action)(items, location) : false;
    }
    return false;
  }

  auto perform(std::span<const TransferItem> items, const DropSession& session)
      -> bool {
    if (auto* received = std::get_if<Received>(&action_);
        received != nullptr && delegate_ == nullptr) {
      if (!*received) {
        return false;
      }
      (*received)(items, session);
      return true;
    }
    return perform(items, session.location());
  }

  void targeted(bool targeted) {
    if (targeted_) {
      targeted_(targeted);
    }
  }

  void entered(std::span<const TransferItem> items, Point location) {
    if (delegate_ != nullptr) {
      delegate_->drop_entered(DropInfo{location, items});
    }
  }

  auto updated(std::span<const TransferItem> items, Point location)
      -> std::optional<DropProposal> {
    if (delegate_ != nullptr) {
      return delegate_->drop_updated(DropInfo{location, items});
    }
    return std::nullopt;
  }

  void exited(std::span<const TransferItem> items, Point location) {
    if (delegate_ != nullptr) {
      delegate_->drop_exited(DropInfo{location, items});
    }
  }

 private:
  std::vector<UTType> accepted_;
  std::variant<Perform, Received> action_;
  Targeted targeted_;
  std::shared_ptr<DropDelegate> delegate_;
  bool enabled_{true};
};

}
