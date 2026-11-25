#include <arisa/controls/text_editor_style.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>

#include <utility>

namespace arisa::controls {

const composition::EnvironmentKey<TextEditorStyle> TextEditorStyle::key{
    "text_editor_style", TextEditorStyle::automatic()};

auto TextEditorStyle::automatic() noexcept -> TextEditorStyle {
  return TextEditorStyle{Kind::automatic};
}

auto TextEditorStyle::plain() noexcept -> TextEditorStyle {
  return TextEditorStyle{Kind::plain};
}

auto TextEditorStyle::rounded_border() noexcept -> TextEditorStyle {
  return TextEditorStyle{Kind::rounded_border};
}

auto operator==(const TextEditorStyle& left, const TextEditorStyle& right)
    -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != TextEditorStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto TextEditorStyle::make_body(Configuration configuration) const
    -> composition::Element {
  switch (kind_) {
    case Kind::automatic:
      if (!configuration.shows_background) {
        return std::move(configuration.content);
      }
      return std::move(configuration.content)
          .background(theme::ColourRole::text_background,
                      theme::Shape::rectangle());
    case Kind::plain:
      return std::move(configuration.content);
    case Kind::rounded_border: {
      const theme::Shape shape =
          theme::Shape::rounded(theme::metrics::text_field.corner_radius, true);
      composition::Element body = std::move(configuration.content);
      if (configuration.shows_background) {
        body = std::move(body).background(theme::ColourRole::text_background,
                                          shape);
      }
      return std::move(body)
          .border(theme::ColourRole::separator, theme::Stroke::hairline())
          .clip_shape(shape);
    }
    case Kind::custom:
      return custom_->make_body(std::move(configuration));
  }
  return std::move(configuration.content);
}

}
