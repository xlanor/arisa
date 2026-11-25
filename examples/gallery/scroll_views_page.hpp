#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/scroll_position.hpp>
#include <arisa/composition/scroll_target.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <cstdint>
#include <string_view>
#include <utility>

namespace arisa::examples {

class ScrollViewsPage final : public composition::Component {
 public:
  enum class Panel : std::uint8_t {
    chat,
    carousel,
    map,
    bounce,
    position,
  };

  class CardSnap {
   public:
    explicit CardSnap(float pitch) noexcept : pitch_(pitch) {}

    void update_target(composition::ScrollTarget& target,
                       const composition::ScrollTargetContext& context) const;

    friend auto operator==(const CardSnap&, const CardSnap&) -> bool = default;

   private:
    float pitch_;
  };

  static constexpr int first_messages = 40;
  static constexpr int card_count = 16;
  static constexpr float card_width = 220.0F;
  static constexpr float card_spacing = 16.0F;
  static constexpr int map_columns = 24;
  static constexpr int map_rows = 16;
  static constexpr float map_tile = 110.0F;
  static constexpr int position_rows = 200;
  static constexpr std::string_view close_button = "scroll-views-close";
  static constexpr std::string_view chat_view = "scroll-views-chat";
  static constexpr std::string_view send_button = "scroll-views-send";
  static constexpr std::string_view carousel_view = "scroll-views-carousel";
  static constexpr std::string_view map_view = "scroll-views-map";
  static constexpr std::string_view short_view = "scroll-views-short";
  static constexpr std::string_view sized_view = "scroll-views-sized";
  static constexpr std::string_view position_view = "scroll-views-position";
  static constexpr composition::StateKey<Panel> panel_key{"scroll-views-panel",
                                                          Panel::chat};
  static constexpr composition::StateKey<int> messages_key{
      "scroll-views-messages", first_messages};
  static const composition::StateKey<composition::ScrollPosition> position_key;

  explicit ScrollViewsPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const ScrollViewsPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto chat(composition::StateHandle<int> messages)
      -> composition::Element;

  static auto carousel() -> composition::Element;

  static auto map() -> composition::Element;

  static auto bounce() -> composition::Element;

  static auto position(
      composition::StateHandle<composition::ScrollPosition> placed)
      -> composition::Element;

  static auto rows(int count) -> composition::Element;

  composition::StateHandle<bool> shown_;
};

}
