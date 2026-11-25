#pragma once

#include <arisa/components/image.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/transfer_item.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::components {

class ShareDestination {
 public:
  struct Request {
    std::vector<composition::TransferItem> items;
    std::optional<std::string> subject;
    std::optional<std::string> message;
  };

  using Perform = std::function<void(const Request&)>;

  static const composition::EnvironmentKey<std::vector<ShareDestination>> key;

  ShareDestination(std::string title, Image::SystemName symbol,
                   Perform perform);

  auto title() const noexcept -> const std::string& { return title_; }

  auto symbol() const noexcept -> const Image::SystemName& { return symbol_; }

  void share(const Request& request) const;

  friend auto operator==(const ShareDestination& left,
                         const ShareDestination& right) -> bool {
    return left.title_ == right.title_ && left.symbol_ == right.symbol_ &&
           left.perform_ == right.perform_;
  }

 private:
  std::string title_;
  Image::SystemName symbol_;
  std::shared_ptr<const Perform> perform_;
};

}

namespace arisa::composition {

template <class Destination>
auto Element::share_destination(Destination destination) && -> Element {
  return std::move(*this).transform_environment(
      components::ShareDestination::key,
      [added = components::ShareDestination{std::move(destination)}](
          std::vector<components::ShareDestination>& list) {
        list.push_back(added);
      });
}

}
