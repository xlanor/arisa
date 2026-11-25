#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::examples {

class FeedPage final : public composition::Component {
 public:
  struct Item {
    int id{0};
    std::string title;
    std::string detail;

    friend auto operator==(const Item&, const Item&) -> bool = default;
  };

  static constexpr int page_size = 15;
  static constexpr int page_count = 4;
  static constexpr int latency_frames = 3;
  static constexpr int prefetch_margin = 5;
  static constexpr std::string_view feed_view = "feed-scroll";
  static constexpr std::string_view loading_row = "feed-loading";

  static const composition::StateKey<std::vector<Item>> items_key;
  static constexpr composition::StateKey<int> requested_key{"feed-requested",
                                                            1};
  static constexpr composition::StateKey<int> refreshes_key{"feed-refreshes",
                                                            0};
  static constexpr composition::StateKey<std::optional<int>> position_key{
      "feed-position", std::nullopt};

  FeedPage(composition::StateHandle<std::vector<Item>> items,
           composition::StateHandle<std::optional<int>> position) noexcept
      : count_(items.get().size()),
        items_(std::move(items)),
        position_(std::move(position)) {}

  auto operator==(const FeedPage& other) const -> bool {
    return count_ == other.count_ &&
           items_.identity() == other.items_.identity() &&
           position_.identity() == other.position_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto fetch(int page, int refreshes) -> std::vector<Item>;

 private:
  static auto load(int page, int refreshes,
                   composition::StateHandle<std::vector<Item>> items)
      -> async::Task<void>;

  static auto reload(composition::StateHandle<std::vector<Item>> items,
                     composition::StateHandle<int> requested,
                     composition::StateHandle<int> refreshes)
      -> async::Task<void>;

  static auto row(const Item& item) -> composition::Element;

  std::size_t count_;
  composition::StateHandle<std::vector<Item>> items_;
  composition::StateHandle<std::optional<int>> position_;
};

}
