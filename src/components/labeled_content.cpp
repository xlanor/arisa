#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/label_column.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/labeled_content_style.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/colour_value.hpp>

#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::components {

LabeledContent::LabeledContent(std::string title, std::string value)
    : Element(std::make_unique<Content>(
          Content::Part{.text = std::move(title), .source = nullptr},
          Content::Part{.text = std::move(value), .source = nullptr})) {}

LabeledContent::LabeledContent(std::string title, composition::Element content)
    : Element(std::make_unique<Content>(
          Content::Part{.text = std::move(title), .source = nullptr},
          Content::Part{
              .text = {},
              .source =
                  std::make_shared<composition::PlaceholderContentView::Source>(
                      std::move(content))})) {}

LabeledContent::LabeledContent(composition::Element content,
                               composition::Element label)
    : Element(std::make_unique<Content>(
          Content::Part{
              .text = {},
              .source =
                  std::make_shared<composition::PlaceholderContentView::Source>(
                      std::move(label))},
          Content::Part{
              .text = {},
              .source =
                  std::make_shared<composition::PlaceholderContentView::Source>(
                      std::move(content))})) {}

auto LabeledContent::Content::Part::element(bool secondary) const
    -> composition::Element {
  if (source != nullptr) {
    return composition::Element{composition::PlaceholderContentView(source)};
  }
  if (secondary) {
    return Text(text).foreground_style(theme::ColourRole::text_secondary);
  }
  return Text(text::LocalizedStringKey{text});
}

auto LabeledContent::column(composition::Context& context)
    -> std::optional<LabelColumn> {
  if (!context.environment(LabeledContentStyle::key).is_automatic()) {
    return std::nullopt;
  }
  return context.environment(LabelColumn::key);
}

auto LabeledContent::columned(const LabelColumn& column,
                              composition::Element label,
                              composition::Element content)
    -> composition::Element {
  return LabeledContentStyle::columned(
      LabeledContentStyle::Configuration{.label = std::move(label),
                                         .content = std::move(content)},
      column);
}

auto LabeledContent::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto LabeledContent::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const LabeledContentStyle& style =
      context.environment(LabeledContentStyle::key);
  const bool hidden =
      context.environment(composition::ControlEnvironment::labels_visibility) ==
      composition::Visibility::hidden;
  std::vector<composition::Element> produced;
  if (const std::optional<LabelColumn> shared = column(context);
      shared.has_value() && !hidden) {
    produced.push_back(
        columned(*shared, label_.element(false), content_.element(true)));
    return produced;
  }
  produced.push_back(style.make_body(LabeledContentStyle::Configuration{
      .label = hidden ? composition::Element{Group()} : label_.element(false),
      .content = content_.element(true)}));
  return produced;
}

}
