#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::examples {

class InteractionScreen final : public composition::Component {
 public:
  class Stage;

  enum class Mode : std::uint8_t {
    toggle,
    slider,
    list,
    menu,
    spinner,
    text,
    clock,
  };

  struct Remote {
    std::optional<composition::StateHandle<std::string>> text;
    std::uint64_t pressed{0};
  };

  static constexpr std::string_view control = "bench-control";
  static constexpr std::string_view stage = "bench-stage";
  static constexpr int list_rows = 50;
  static constexpr float card_width = 420.0F;
  static constexpr float list_height = 240.0F;
  static constexpr float tile_width = 118.0F;
  static constexpr float tile_height = 64.0F;
  static constexpr double slider_step = 0.05;

  InteractionScreen(Mode mode, std::shared_ptr<Remote> remote) noexcept
      : mode_(mode), remote_(std::move(remote)) {}

  auto operator==(const InteractionScreen&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto tiles() -> composition::Element;

  Mode mode_;
  std::shared_ptr<Remote> remote_;
};

class InteractionScreen::Stage final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> flag{"bench-flag", false};
  static constexpr composition::StateKey<double> level{"bench-level", 0.5};
  static const composition::StateKey<std::string> typed;

  Stage(Mode mode, std::shared_ptr<Remote> remote) noexcept
      : mode_(mode), remote_(std::move(remote)) {}

  auto operator==(const Stage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto card(composition::Element content) -> composition::Element;

  static auto rows(const std::shared_ptr<Remote>& remote)
      -> composition::Element;

  static auto actions(const std::shared_ptr<Remote>& remote)
      -> composition::Element;

  Mode mode_;
  std::shared_ptr<Remote> remote_;
};

}
