#pragma once

#include <arisa/attr/focus.hpp>
#include <arisa/attr/typography.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label_style.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/image_attributes.hpp>
#include <arisa/composition/search_field_placement.hpp>
#include <arisa/composition/search_scope_activation.hpp>
#include <arisa/composition/submit_triggers.hpp>
#include <arisa/composition/symbol_variants.hpp>
#include <arisa/controls/date_picker_style.hpp>
#include <arisa/controls/gauge_style.hpp>
#include <arisa/controls/menu_style.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/controls/searchable.hpp>
#include <arisa/controls/text_field_style.hpp>
#include <arisa/controls/toggle_style.hpp>
#include <arisa/theme/typography.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace arisa::attr {

struct ImageOptions {
  bool resizable{false};
  std::optional<theme::Font> font;
  std::optional<components::Image::TemplateRenderingMode> rendering_mode;
  std::optional<components::Image::Interpolation> interpolation;
};

class RenderingMode {
 public:
  constexpr explicit RenderingMode(
      components::Image::TemplateRenderingMode mode) noexcept
      : mode_(mode) {}

  constexpr void configure(ImageOptions& options) const noexcept {
    options.rendering_mode = mode_;
  }

 private:
  components::Image::TemplateRenderingMode mode_;
};

class Interpolation {
 public:
  constexpr explicit Interpolation(
      components::Image::Interpolation value) noexcept
      : value_(value) {}

  constexpr void configure(ImageOptions& options) const noexcept {
    options.interpolation = value_;
  }

 private:
  components::Image::Interpolation value_;
};

using rendering_mode = RenderingMode;
using interpolation = Interpolation;
using image_scale =
    TextValue<composition::ImageScale, &composition::Element::image_scale>;
using symbol_variant = TextValue<composition::SymbolVariants,
                                 &composition::Element::symbol_variant>;
using symbol_rendering_mode =
    TextValue<composition::SymbolRenderingMode,
              &composition::Element::symbol_rendering_mode>;

class Resizable {
 public:
  constexpr void configure(ImageOptions& options) const noexcept {
    options.resizable = true;
  }
};

class ButtonStyleAttribute {
 public:
  explicit ButtonStyleAttribute(components::ButtonStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).button_style(std::move(style_));
  }

 private:
  components::ButtonStyle style_;
};

class ToggleStyleAttribute {
 public:
  explicit ToggleStyleAttribute(controls::ToggleStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).toggle_style(std::move(style_));
  }

 private:
  controls::ToggleStyle style_;
};

class PickerStyleAttribute {
 public:
  explicit PickerStyleAttribute(controls::PickerStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).picker_style(std::move(style_));
  }

 private:
  controls::PickerStyle style_;
};

class ProgressViewStyleAttribute {
 public:
  explicit ProgressViewStyleAttribute(controls::ProgressViewStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).progress_view_style(std::move(style_));
  }

 private:
  controls::ProgressViewStyle style_;
};

class LabelStyleAttribute {
 public:
  explicit LabelStyleAttribute(components::LabelStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).label_style(std::move(style_));
  }

 private:
  components::LabelStyle style_;
};

class TextFieldStyleAttribute {
 public:
  explicit TextFieldStyleAttribute(controls::TextFieldStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).text_field_style(std::move(style_));
  }

 private:
  controls::TextFieldStyle style_;
};

class TagAttribute {
 public:
  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  explicit TagAttribute(T value)
      : write_([held = std::move(value)](composition::Element element) {
          return std::move(element).tag(held);
        }) {}

  auto apply(composition::Element element) && -> composition::Element {
    return write_(std::move(element));
  }

 private:
  std::move_only_function<composition::Element(composition::Element)> write_;
};

class OnSubmit {
 public:
  explicit OnSubmit(std::move_only_function<void()> perform) noexcept
      : action_(std::move(perform)) {}

  OnSubmit(composition::SubmitTriggers triggers,
           std::move_only_function<void()> perform) noexcept
      : triggers_(triggers), action_(std::move(perform)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).on_submit(triggers_, std::move(action_));
  }

 private:
  composition::SubmitTriggers triggers_{composition::SubmitTriggers::text};
  std::move_only_function<void()> action_;
};

class SearchableAttribute : public Deferred {
 public:
  template <composition::BindingSource Text>
  explicit SearchableAttribute(Text text,
                               composition::SearchFieldPlacement placement =
                                   composition::SearchFieldPlacement::automatic,
                               std::optional<std::string> prompt = std::nullopt)
      : Deferred([text = composition::Binding<std::string>{std::move(text)},
                  placement, prompt = std::move(prompt)](
                     composition::Element element) mutable {
          return std::move(element).searchable(std::move(text), placement,
                                               std::move(prompt));
        }) {}

  template <composition::BindingSource Text>
  SearchableAttribute(Text text, std::string prompt)
      : SearchableAttribute(std::move(text),
                            composition::SearchFieldPlacement::automatic,
                            std::move(prompt)) {}
};

class SearchSuggestions : public Deferred {
 public:
  template <class Suggestions>
    requires std::invocable<const Suggestions&>
  explicit SearchSuggestions(Suggestions suggestions)
      : Deferred([held = std::move(suggestions)](
                     composition::Element element) mutable {
          return std::move(element).search_suggestions(std::move(held));
        }) {}
};

class SearchScopes : public Deferred {
 public:
  template <composition::BindingSource Scope, class Scopes>
  SearchScopes(Scope scope, Scopes scopes,
               composition::SearchScopeActivation activation =
                   composition::SearchScopeActivation::automatic)
      : Deferred([scope = std::move(scope), held = std::move(scopes),
                  activation](composition::Element element) mutable {
          return std::move(element).search_scopes(std::move(scope),
                                                  std::move(held), activation);
        }) {}
};

class SearchCompletion : public Deferred {
 public:
  template <class Completion>
  explicit SearchCompletion(Completion completion)
      : Deferred([held = std::move(completion)](
                     composition::Element element) mutable {
          return std::move(element).search_completion(std::move(held));
        }) {}
};

class SearchFocused : public Deferred {
 public:
  explicit SearchFocused(composition::FocusBinding<bool> condition)
      : Deferred([condition = std::move(condition)](
                     composition::Element element) mutable {
          return std::move(element).search_focused(std::move(condition));
        }) {}
};

class ControlSizeAttribute {
 public:
  constexpr explicit ControlSizeAttribute(components::ControlSize size) noexcept
      : size_(size) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).control_size(size_);
  }

 private:
  components::ControlSize size_;
};

class LabelsHidden {
 public:
  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).labels_hidden();
  }
};

class MenuStyleAttribute {
 public:
  explicit MenuStyleAttribute(controls::MenuStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).menu_style(std::move(style_));
  }

 private:
  controls::MenuStyle style_;
};

class GaugeStyleAttribute {
 public:
  explicit GaugeStyleAttribute(controls::GaugeStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).gauge_style(std::move(style_));
  }

 private:
  controls::GaugeStyle style_;
};

class DatePickerStyleAttribute {
 public:
  explicit DatePickerStyleAttribute(controls::DatePickerStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).date_picker_style(std::move(style_));
  }

 private:
  controls::DatePickerStyle style_;
};

class SubmitScope {
 public:
  constexpr explicit SubmitScope(bool is_blocking = true) noexcept
      : blocking_(is_blocking) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).submit_scope(blocking_);
  }

 private:
  bool blocking_;
};

class SubmitLabelAttribute {
 public:
  constexpr explicit SubmitLabelAttribute(provider::SubmitLabel label) noexcept
      : label_(label) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).submit_label(label_);
  }

 private:
  provider::SubmitLabel label_;
};

class KeyboardTypeAttribute {
 public:
  constexpr explicit KeyboardTypeAttribute(provider::KeyboardType type) noexcept
      : type_(type) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).keyboard_type(type_);
  }

 private:
  provider::KeyboardType type_;
};

class TextContentTypeAttribute {
 public:
  constexpr explicit TextContentTypeAttribute(
      std::optional<provider::TextContentType> type) noexcept
      : type_(type) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).text_content_type(type_);
  }

 private:
  std::optional<provider::TextContentType> type_;
};

class AutocapitalizationAttribute {
 public:
  constexpr explicit AutocapitalizationAttribute(
      std::optional<provider::TextInputAutocapitalization> style) noexcept
      : style_(style) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).text_input_autocapitalization(style_);
  }

 private:
  std::optional<provider::TextInputAutocapitalization> style_;
};

class AutocorrectionDisabled {
 public:
  constexpr explicit AutocorrectionDisabled(bool disable = true) noexcept
      : disable_(disable) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).autocorrection_disabled(disable_);
  }

 private:
  bool disable_;
};

inline constexpr Resizable resizable{};

using button_style = ButtonStyleAttribute;
using toggle_style = ToggleStyleAttribute;
using picker_style = PickerStyleAttribute;
using progress_view_style = ProgressViewStyleAttribute;
using label_style = LabelStyleAttribute;
using text_field_style = TextFieldStyleAttribute;
using tag = TagAttribute;
using on_submit = OnSubmit;
using searchable = SearchableAttribute;
using search_suggestions = SearchSuggestions;
using search_scopes = SearchScopes;
using search_completion = SearchCompletion;
using search_focused = SearchFocused;
using submit_scope = SubmitScope;
using submit_label = SubmitLabelAttribute;
using keyboard_type = KeyboardTypeAttribute;
using text_content_type = TextContentTypeAttribute;
using text_input_autocapitalization = AutocapitalizationAttribute;
using autocorrection_disabled = AutocorrectionDisabled;
using system_name = components::Image::SystemName;
using control_size = ControlSizeAttribute;
using labels_hidden = LabelsHidden;
using menu_style = MenuStyleAttribute;
using gauge_style = GaugeStyleAttribute;
using date_picker_style = DatePickerStyleAttribute;

}
