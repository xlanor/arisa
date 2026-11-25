#include <arisa/theme/typography.hpp>

#include <utility>

namespace arisa::theme {

auto Font::resolve(const Context& context) const -> Resolved {
  const TypeStyle& type = context.style(role_);
  const TypeStyle& base = context.base(role_);
  float size = type.size;
  if (size_.has_value()) {
    size = *size_;
    if (name_.has_value() && !fixed_ && base.size > 0.0F) {
      size *= type.size / base.size;
    }
  }
  size *= scale_;
  Weight chosen = weight_.value_or(static_cast<Weight>(type.weight));
  if (context.bold_text() && std::to_underlying(chosen) < 700) {
    chosen = static_cast<Weight>(std::to_underlying(chosen) + 300 > 900
                                     ? 900
                                     : std::to_underlying(chosen) + 300);
  }
  const bool monospaced = design_ == Design::monospaced;
  Resolved resolved{
      .point_size = size,
      .weight = chosen,
      .width = width_.value_or(Width::standard),
      .leading = leading_,
      .is_bold = std::to_underlying(chosen) >= std::to_underlying(Weight::bold),
      .is_italic = italic_,
      .is_monospaced = monospaced,
      .is_small_caps = small_caps_ == text::SmallCaps::all,
      .is_lowercase_small_caps = text::SmallCapitals::lowers(small_caps_),
      .is_uppercase_small_caps = text::SmallCapitals::uppers(small_caps_),
      .face = text::FaceQuery{
          .weight = text::FontWeight::create(std::to_underlying(chosen))
                        .value_or(text::font_weight::regular),
          .design = type.design == TypeDesign::display
                        ? text::FontDesign::display
                        : text::FontDesign::text,
          .italic = italic_,
          .typeface = text::Typeface::standard,
          .stretch = width_.value_or(Width::standard).stretch(),
          .name = name_}};
  if (design_.has_value()) {
    resolved.face.typeface = static_cast<text::Typeface>(*design_);
  }
  return resolved;
}

}
