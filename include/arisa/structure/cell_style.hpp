#pragma once

#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/metrics.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::structure {

class DefaultCellStyle;

class CellStyle {
 public:
  struct Configuration {
    composition::Element content;
    ListStyle list_style{ListStyle::automatic()};
    bool is_first{true};
    bool is_last{true};
    bool is_focused{false};
    bool is_selected{false};
    bool is_lifted{false};
    bool shows_disclosure{false};
    bool fills{false};
    std::optional<theme::Insets> insets{};
    bool shows_separator{true};
    std::optional<theme::ColourValue> separator_tint{};
    std::optional<composition::Element> background{};
    std::optional<float> minimum_height{};
    std::optional<float> corner_radius{};
  };

  static const composition::EnvironmentKey<CellStyle> key;

  static constexpr float minimum_height =
      theme::metrics::list.grouped_row_height;
  static constexpr float lifted_scale = 1.03F;
  static constexpr float chevron_extent = 13.0F;
  static constexpr theme::Colour clear{0.0F, 0.0F, 0.0F, 0.0F};

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, CellStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, DefaultCellStyle> &&
             components::StyleOf<Style, Configuration>)
  CellStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  CellStyle(DefaultCellStyle style) noexcept;

  static auto automatic() noexcept -> CellStyle;

  auto make_body(Configuration configuration) const -> composition::Element;

  static auto insets() noexcept -> theme::Insets;

  static auto insets(const ListStyle& list) noexcept -> theme::Insets;

  static auto row_height(const ListStyle& list) noexcept -> theme::ScaledMetric;

  static auto default_row_height(const ListStyle& list) noexcept -> float;

  static auto section_separator(std::optional<theme::ColourValue> tint)
      -> composition::Element;

  friend auto operator==(const CellStyle& left, const CellStyle& right) -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    custom,
  };

  explicit CellStyle(Kind kind) noexcept : kind_(kind) {}

  static auto row(Configuration& configuration) -> composition::Element;

  static auto grouped(Configuration configuration) -> composition::Element;

  static auto plain(Configuration configuration) -> composition::Element;

  static auto sidebar(Configuration configuration) -> composition::Element;

  static auto card(bool first, bool last, float radius) -> composition::Element;

  static auto fill(Configuration& configuration, bool first, bool last,
                   float radius) -> composition::Element;

  static auto separator(theme::Space inset,
                        std::optional<theme::ColourValue> tint)
      -> composition::Element;

  static auto separated(composition::Element body,
                        const Configuration& configuration, bool hidden)
      -> composition::Element;

  static auto lift(composition::Element element, bool lifted)
      -> composition::Element;

  Kind kind_;
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

class DefaultCellStyle {
 public:
  auto make_body(CellStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const DefaultCellStyle&, const DefaultCellStyle&)
      -> bool = default;
};

}

namespace arisa::composition {

template <class Style>
auto Element::cell_style(Style style) && -> Element {
  return std::move(*this).environment(structure::CellStyle::key,
                                      structure::CellStyle{std::move(style)});
}

}
