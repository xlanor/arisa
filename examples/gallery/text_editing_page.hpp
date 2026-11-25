#pragma once

#include <arisa/components/attributed_string.hpp>
#include <arisa/components/attributed_text_selection.hpp>
#include <arisa/components/graphics_context.hpp>
#include <arisa/components/text_layout.hpp>
#include <arisa/components/text_renderer.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/text_selection.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::examples {

class TextEditingPage final : public composition::Component {
 public:
  class Opener;
  class Wave;

  static constexpr std::string_view close_button = "text-editing-close";
  static constexpr std::string_view field = "text-editing-field";
  static constexpr std::string_view editor = "text-editing-editor";
  static constexpr std::string_view rich = "text-editing-rich";
  static constexpr std::string_view selectable = "text-editing-selectable";
  static constexpr std::string_view wave = "text-editing-wave";
  static constexpr std::string_view bordered = "text-editing-bordered";
  static constexpr std::string_view square = "text-editing-square";
  static constexpr std::string_view capsule = "text-editing-capsule";
  static constexpr std::string_view date = "text-editing-date";
  static constexpr std::string_view bytes = "text-editing-bytes";
  static constexpr std::string_view price = "text-editing-price";
  static constexpr std::string_view city = "text-editing-city";
  static constexpr std::string_view find_button = "text-editing-find";
  static constexpr std::string_view done_button = "text-editing-done";

  static inline const composition::StateKey<std::string> name{
      "text-editing-name", "Ada Lovelace"};
  static inline const composition::StateKey<
      std::optional<composition::TextSelection>>
      selection{"text-editing-selection", std::nullopt};
  static inline const composition::StateKey<std::string> notes{
      "text-editing-notes", "The quick brown fox\njumps over the lazy dog"};
  static inline const composition::StateKey<components::AttributedString>
      styled{"text-editing-styled", components::AttributedString::markdown(
                                        "Select **words** to style")};
  static inline const composition::StateKey<components::AttributedTextSelection>
      styled_selection{"text-editing-styled-selection",
                       components::AttributedTextSelection{}};
  static constexpr composition::StateKey<std::int64_t> size{"text-editing-size",
                                                            1536000};
  static constexpr composition::StateKey<double> cost{"text-editing-cost", 4.5};
  static inline const composition::StateKey<std::chrono::sys_seconds> day{
      "text-editing-day",
      std::chrono::sys_seconds{std::chrono::sys_days{
          std::chrono::year{2026} / std::chrono::November / 17}}};
  static constexpr composition::StateKey<std::string> town{"text-editing-town",
                                                           ""};
  static constexpr composition::StateKey<bool> finding{"text-editing-finding",
                                                       false};

  explicit TextEditingPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const TextEditingPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto section(std::string_view title, composition::Element content)
      -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class TextEditingPage::Wave final : public components::TextRenderer {
 public:
  static constexpr float amplitude = 3.0F;

  void draw(const components::Text::Layout& layout,
            components::GraphicsContext& context) const override;

  auto display_padding() const -> theme::EdgeInsets override;
};

class TextEditingPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"text-editing-shown",
                                                         false};
  static constexpr std::string_view button = "text-editing-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
