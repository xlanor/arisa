#include <arisa/provider/text_input/text_input_traits.hpp>

namespace arisa::provider {

auto TextInputTraitSet::name(TextInputTrait trait) noexcept
    -> std::string_view {
  switch (trait) {
    case TextInputTrait::keyboard_type:
      return "keyboard type";
    case TextInputTrait::text_content_type:
      return "text content type";
    case TextInputTrait::autocapitalization:
      return "autocapitalization";
    case TextInputTrait::autocorrection:
      return "autocorrection";
    case TextInputTrait::submit_label:
      return "submit label";
    case TextInputTrait::multiline:
      return "multiline";
  }
  return "keyboard type";
}

auto TextInputTraits::submit_text(SubmitLabel label) noexcept
    -> std::string_view {
  switch (label) {
    case SubmitLabel::done:
      return "Done";
    case SubmitLabel::go:
      return "Go";
    case SubmitLabel::send:
      return "Send";
    case SubmitLabel::join:
      return "Join";
    case SubmitLabel::route:
      return "Route";
    case SubmitLabel::search:
      return "Search";
    case SubmitLabel::return_key:
      return "";
    case SubmitLabel::next:
      return "Next";
    case SubmitLabel::continue_key:
      return "Continue";
  }
  return "";
}

auto TextInputTraits::requested() const noexcept -> TextInputTraitSet {
  TextInputTraitSet asked;
  if (keyboard != KeyboardType::automatic) {
    asked.insert(TextInputTrait::keyboard_type);
  }
  if (content.has_value()) {
    asked.insert(TextInputTrait::text_content_type);
  }
  if (capitalization.has_value()) {
    asked.insert(TextInputTrait::autocapitalization);
  }
  if (autocorrection_disabled.has_value()) {
    asked.insert(TextInputTrait::autocorrection);
  }
  if (submit_label != SubmitLabel::return_key) {
    asked.insert(TextInputTrait::submit_label);
  }
  if (multiline) {
    asked.insert(TextInputTrait::multiline);
  }
  return asked;
}

}
