#pragma once

#include <arisa/components/text_layout.hpp>
#include <arisa/components/text_layout_key.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/key_press.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/text_environment.hpp>
#include <arisa/composition/text_selectability.hpp>
#include <arisa/composition/text_selection.hpp>
#include <arisa/theme/colour_value.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::controls {

class TextSelectionHost : public composition::Element {
 public:
  class Content;

  using Range = composition::TextSelection::Range;

  struct Selected {
    std::size_t layout{0};
    Range range{};

    friend auto operator==(const Selected&, const Selected&) -> bool = default;
  };

  struct Placed {
    composition::Point origin{};
    components::Text::Layout layout{};
  };

  enum class End : std::uint8_t {
    lower,
    upper,
  };

  using Moved = std::function<void(End end, std::size_t index)>;

  static constexpr float handle_size = 10.0F;
  static constexpr float highlight_opacity = 1.0F;
  static constexpr std::string_view copy_label = "Copy";
  static constexpr std::string_view select_all_label = "Select All";

  explicit TextSelectionHost(composition::Element content);

  static auto marks(const components::Text::LayoutKey::Value& layouts,
                    std::size_t which, Range range, theme::ColourValue colour)
      -> composition::Element;

  static auto caret(const components::Text::LayoutKey::Value& layouts,
                    std::size_t which, std::size_t index,
                    theme::ColourValue colour) -> composition::Element;

  static auto handles(const components::Text::LayoutKey::Value& layouts,
                      std::size_t which, Range range, theme::ColourValue colour,
                      Moved moved) -> composition::Element;

  static auto placed(const components::Text::LayoutKey::Value& layouts,
                     const composition::GeometryProxy& proxy)
      -> std::vector<Placed>;

  static auto index_at(const std::vector<Placed>& placed, composition::Point at)
      -> std::optional<Selected>;
};

class TextSelectionHost::Content final : public composition::ElementContent {
 public:
  static const composition::StateKey<std::optional<Selected>> selection;
  static const composition::StateKey<bool> menu;
  static const composition::StateKey<components::Text::LayoutKey::Value> seen;

  explicit Content(composition::Element content);

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  static auto selected_text(const components::Text::LayoutKey::Value& layouts,
                            const std::optional<Selected>& selected)
      -> std::string;

 private:
  std::shared_ptr<composition::PlaceholderContentView::Source> content_;
};

}

namespace arisa::composition {

template <class Marker>
auto Element::text_selection(Marker) && -> Element {
  static_assert(Selectability<Marker>);
  if constexpr (Marker::allows_selection) {
    return controls::TextSelectionHost(std::move(*this));
  } else {
    return std::move(*this).environment(TextEnvironment::selectable, false);
  }
}

}
