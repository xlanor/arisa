#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/overlay_input.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/composition/popover_placement.hpp>
#include <arisa/composition/scene_preferences.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/theme/space.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace arisa::composition {

class Layer : public Element {
 public:
  struct Anchoring {
    PopoverAttachmentAnchor attachment{PopoverAttachmentAnchor::rect()};
    theme::Edge arrow_edge{theme::Edge::top};
    bool arrow{true};

    friend constexpr auto operator==(const Anchoring&, const Anchoring&)
        -> bool = default;
  };

  class Content;

  static constexpr float anchor_gap = 6.0F;
  static constexpr float arrow_size = 10.0F;
  static constexpr float safe_margin = 16.0F;

  static auto default_animation() noexcept -> motion::Animation;

  Layer(OverlayLayer layer, OverlayInput input, Alignment alignment,
        std::vector<Element> children,
        std::optional<Anchoring> anchoring = std::nullopt,
        std::optional<PopoverPlacement> placement = std::nullopt);
};

class Layer::Content final : public ElementContent {
 public:
  Content(OverlayLayer layer, OverlayInput input, Alignment alignment,
          std::optional<Anchoring> anchoring,
          std::optional<PopoverPlacement> placement = std::nullopt) noexcept
      : layer_(layer),
        input_(input),
        alignment_(alignment),
        anchoring_(anchoring),
        placement_(std::move(placement)) {}

  auto type() const noexcept -> ElementType override;

  auto paints_like(const ElementContent& other) const -> bool override;

  auto isolates(Identity preference) const noexcept -> bool override {
    return !ScenePreferences::crosses(preference);
  }

  auto layer() const noexcept -> OverlayLayer { return layer_; }

  auto input() const noexcept -> OverlayInput { return input_; }

  auto alignment() const noexcept -> Alignment { return alignment_; }

  auto anchoring() const noexcept -> const std::optional<Anchoring>& {
    return anchoring_;
  }

  auto placement() const noexcept -> const std::optional<PopoverPlacement>& {
    return placement_;
  }

 private:
  OverlayLayer layer_;
  OverlayInput input_;
  Alignment alignment_;
  std::optional<Anchoring> anchoring_;
  std::optional<PopoverPlacement> placement_;
};

}
