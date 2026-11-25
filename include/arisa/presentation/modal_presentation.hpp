#pragma once

#include <arisa/composition/size.hpp>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/presentation_background.hpp>
#include <arisa/composition/presentation_detent.hpp>
#include <arisa/composition/presentation_options.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/presentation/presentation_layer.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/color_scheme.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace arisa::presentation {

class ModalPresentation final : public composition::Component {
 public:
  enum class Kind : std::uint8_t {
    sheet,
    popover,
  };

  static constexpr composition::StateKey<int> detent_key{"arisa.sheet.detent",
                                                         0};
  static constexpr composition::StateKey<float> pull_key{"arisa.sheet.pull",
                                                         0.0F};
  static constexpr composition::StateKey<float> extent_key{"arisa.sheet.extent",
                                                           0.0F};
  static constexpr composition::StateKey<bool> confirming_key{
      "arisa.sheet.confirming", false};
  static const composition::StateKey<std::shared_ptr<composition::Size>>
      largest_key;
  static constexpr composition::StateKey<std::optional<theme::ColorScheme>>
      scheme_key{"arisa.sheet.scheme", std::nullopt};
  static constexpr provider::Button resize_button =
      provider::Button::right_stick;
  static constexpr std::string_view resize_label = "Resize";
  static constexpr float grabber_width = 36.0F;
  static constexpr float grabber_height = 5.0F;
  static constexpr float grabber_strip = 20.0F;
  static constexpr float dismiss_fraction = 0.5F;

  ModalPresentation(Kind kind, PresentationLayer::Presence presence,
                    PresentationLayer::Dismissed on_dismiss,
                    composition::Element content,
                    std::optional<composition::Layer::Anchoring> anchoring);

  auto operator==(const ModalPresentation& other) const -> bool {
    return source_ == other.source_;
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  struct Detents {
    std::vector<composition::PresentationDetent> ordered;
    std::size_t index{0};
    float largest{0.0F};
  };

  auto options() const -> const composition::PresentationOptions&;

  auto content() const -> composition::Element;

  auto dismissed() const -> PresentationLayer::Dismissed;

  auto sheet(composition::Context& context) const -> composition::Element;

  auto detents(float screen, composition::StateHandle<int> chosen) const
      -> Detents;

  auto largest(float screen) const -> float;

  auto exit(composition::StateHandle<bool> confirming) const
      -> std::function<void()>;

  auto confirmed(composition::Element panel,
                 composition::StateHandle<bool> confirming) const
      -> composition::Element;

  auto select(composition::StateHandle<int> chosen,
              std::vector<composition::PresentationDetent> ordered) const
      -> std::function<void(std::size_t)>;

  static auto grabber(bool visible) -> composition::Element;

  static auto nearest(
      const std::vector<composition::PresentationDetent>& ordered,
      float largest, float height) -> std::size_t;

  Kind kind_;
  PresentationLayer::Presence presence_;
  std::shared_ptr<PresentationLayer::Dismissed> on_dismiss_;
  std::shared_ptr<const composition::PresentationOptions> options_;
  std::optional<composition::PresentationBackground> background_;
  bool locked_{false};
  std::optional<composition::Layer::Anchoring> anchoring_;
  std::shared_ptr<composition::PlaceholderContentView::Source> source_;
};

}
