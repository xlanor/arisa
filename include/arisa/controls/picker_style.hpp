#pragma once

#include <arisa/components/control_size.hpp>
#include <arisa/components/style_box.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/command.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/palette_selection_effect.hpp>
#include <arisa/theme/metrics.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::controls {

class DefaultPickerStyle;
class SegmentedPickerStyle;
class TabsPickerStyle;
class MenuPickerStyle;
class InlinePickerStyle;
class RadioGroupPickerStyle;
class NavigationLinkPickerStyle;
class WheelPickerStyle;
class PalettePickerStyle;

class PickerStyle {
 public:
  struct Option {
    composition::Element label;
    bool is_selected{false};
    std::function<void()> select;
    bool is_mixed{false};
  };

  using Options = std::function<std::vector<Option>()>;

  struct Configuration {
    composition::Element label;
    std::string title;
    Options options;
    bool is_enabled{true};
    components::ControlSize control_size{components::ControlSize::regular};
    PaletteSelectionEffect palette_effect{PaletteSelectionEffect::automatic};
    composition::Axis radio_group_layout{composition::Axis::vertical};
  };

  class Segmented;
  class Chooser;
  class Page;
  class Wheel;
  class Palette;

  static const composition::EnvironmentKey<PickerStyle> key;

  PickerStyle(DefaultPickerStyle style) noexcept;

  PickerStyle(SegmentedPickerStyle style) noexcept;

  PickerStyle(TabsPickerStyle style) noexcept;

  PickerStyle(MenuPickerStyle style) noexcept;

  PickerStyle(InlinePickerStyle style) noexcept;

  PickerStyle(RadioGroupPickerStyle style) noexcept;

  PickerStyle(NavigationLinkPickerStyle style) noexcept;

  PickerStyle(WheelPickerStyle style) noexcept;

  PickerStyle(PalettePickerStyle style) noexcept;

  static constexpr float indicator_extent = 20.0F;
  static constexpr float tabs_segment_padding = 5.0F;
  static constexpr float radio_group_spacing = 8.0F;
  static constexpr std::string_view mixed_label = "Mixed";

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, PickerStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, DefaultPickerStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, SegmentedPickerStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, TabsPickerStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, MenuPickerStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, InlinePickerStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, RadioGroupPickerStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           NavigationLinkPickerStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, WheelPickerStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, PalettePickerStyle> &&
             components::StyleOf<Style, Configuration>)
  PickerStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> PickerStyle;

  static auto segmented() noexcept -> PickerStyle;

  static auto tabs() noexcept -> PickerStyle;

  static auto menu() noexcept -> PickerStyle;

  static auto radio_group() noexcept -> PickerStyle;

  static auto navigation_link() noexcept -> PickerStyle;

  static auto wheel() noexcept -> PickerStyle;

  static auto palette() noexcept -> PickerStyle;

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const PickerStyle& left, const PickerStyle& right)
      -> bool;

  static auto selected_index(const std::vector<Option>& options)
      -> std::optional<std::size_t>;

  static auto row(Option option, bool radio, std::function<void()> after)
      -> composition::Element;

  static auto stepping(std::vector<Option>& options, composition::Axis axis)
      -> std::shared_ptr<composition::Command>;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    segmented,
    tabs,
    menu,
    inline_list,
    radio_group,
    navigation_link,
    wheel,
    palette,
    custom,
  };

  explicit PickerStyle(Kind kind) noexcept : kind_(kind) {}

  static auto menued(Configuration configuration) -> composition::Element;

  static auto listed(Configuration configuration, bool radio)
      -> composition::Element;

  static auto radio(bool selected) -> composition::Element;

  static auto marked(Option option, bool radio, std::function<void()> after,
                     bool fills) -> composition::Element;

  Kind kind_;
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

class PickerStyle::Segmented : public composition::Element {
 public:
  class Content;

  static constexpr float height = theme::metrics::segmented.height;

  explicit Segmented(
      std::vector<Option> options,
      components::ControlSize size = components::ControlSize::regular,
      float extra_padding = 0.0F);

 private:
  static auto segments(std::vector<Option>& options,
                       components::ControlSize size, float extra_padding)
      -> std::vector<composition::Element>;
};

class PickerStyle::Segmented::Content final
    : public composition::ElementContent {
 public:
  auto type() const noexcept -> composition::ElementType override;

  auto framed() const noexcept -> bool override { return true; }
};

class PickerStyle::Chooser : public composition::Element {
 public:
  class Content;

  Chooser(composition::Element label, std::string title, Options options);

  static auto rows(const Options& options, const std::function<void()>& after)
      -> composition::Element;
};

class PickerStyle::Chooser::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<bool> shown{"picker-shown", false};

  Content(Source label, std::string title, Options options) noexcept
      : label_(std::move(label)),
        title_(std::move(title)),
        options_(std::move(options)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  Source label_;
  std::string title_;
  Options options_;
};

class PickerStyle::Page : public composition::Element {
 public:
  class Content;

  Page(std::string title, Options options);
};

class PickerStyle::Page::Content final : public composition::ElementContent {
 public:
  Content(std::string title, Options options) noexcept
      : title_(std::move(title)), options_(std::move(options)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  std::string title_;
  Options options_;
};

class PickerStyle::Wheel : public composition::Element {
 public:
  class Content;

  static constexpr float row_height = 32.0F;
  static constexpr std::size_t reach = 2;
  static constexpr float fade_step = 0.3F;
  static constexpr float shrink_step = 0.08F;
  static constexpr float drum_radius = 64.0F;
  static constexpr float flick_seconds_per_row = 0.06F;
  static constexpr float flick_base_seconds = 0.25F;
  static constexpr float flick_limit_seconds = 0.9F;
  static constexpr std::string_view adjust_label = "Adjust";
  static constexpr std::string_view done_label = "Done";

  explicit Wheel(std::vector<Option> options);
};

class PickerStyle::Wheel::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  using Selects = std::shared_ptr<const std::vector<std::function<void()>>>;

  struct Slot {
    Source label;
    bool is_selected{false};
    std::function<void()> select;
  };

  static constexpr composition::StateKey<bool> engaged{"picker-wheel-engaged",
                                                       false};
  static constexpr composition::StateKey<float> dragged{"picker-wheel-dragged",
                                                        0.0F};

  explicit Content(std::vector<Slot> slots) noexcept
      : slots_(std::move(slots)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  auto row(std::ptrdiff_t index, std::size_t centre) const
      -> composition::Element;

  static void step(const Selects& selects, std::size_t centre,
                   std::ptrdiff_t by);

  std::vector<Slot> slots_;
};

class PickerStyle::Palette : public composition::Element {
 public:
  class Content;

  static constexpr float swatch_extent = 36.0F;

  explicit Palette(
      std::vector<Option> options,
      PaletteSelectionEffect effect = PaletteSelectionEffect::automatic);

 private:
  static auto swatches(std::vector<Option>& options,
                       PaletteSelectionEffect effect)
      -> std::vector<composition::Element>;
};

class PickerStyle::Palette::Content final : public composition::ElementContent {
 public:
  auto type() const noexcept -> composition::ElementType override;

  auto framed() const noexcept -> bool override { return true; }
};

class DefaultPickerStyle {
 public:
  auto make_body(PickerStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const DefaultPickerStyle&, const DefaultPickerStyle&)
      -> bool = default;
};

class SegmentedPickerStyle {
 public:
  auto make_body(PickerStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const SegmentedPickerStyle&,
                         const SegmentedPickerStyle&) -> bool = default;
};

class TabsPickerStyle {
 public:
  TabsPickerStyle() noexcept = default;

  auto make_body(PickerStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const TabsPickerStyle&, const TabsPickerStyle&)
      -> bool = default;
};

class MenuPickerStyle {
 public:
  auto make_body(PickerStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const MenuPickerStyle&, const MenuPickerStyle&)
      -> bool = default;
};

class InlinePickerStyle {
 public:
  auto make_body(PickerStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const InlinePickerStyle&, const InlinePickerStyle&)
      -> bool = default;
};

class RadioGroupPickerStyle {
 public:
  auto make_body(PickerStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const RadioGroupPickerStyle&,
                         const RadioGroupPickerStyle&) -> bool = default;
};

class NavigationLinkPickerStyle {
 public:
  auto make_body(PickerStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const NavigationLinkPickerStyle&,
                         const NavigationLinkPickerStyle&) -> bool = default;
};

class WheelPickerStyle {
 public:
  auto make_body(PickerStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const WheelPickerStyle&, const WheelPickerStyle&)
      -> bool = default;
};

class PalettePickerStyle {
 public:
  auto make_body(PickerStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const PalettePickerStyle&, const PalettePickerStyle&)
      -> bool = default;
};

}

namespace arisa::composition {

template <class Style>
auto Element::picker_style(Style style) && -> Element {
  return std::move(*this).environment(controls::PickerStyle::key,
                                      controls::PickerStyle{std::move(style)});
}

}
