#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/structure/group_box.hpp>
#include <arisa/structure/group_box_style.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <memory>
#include <utility>

namespace arisa::structure {

GroupBox::GroupBox(composition::Element content)
    : GroupBox(std::move(content), components::Group()) {}

GroupBox::GroupBox(std::string title, composition::Element content)
    : GroupBox(std::move(content),
               components::Text(text::LocalizedStringKey{std::move(title)})) {}

GroupBox::GroupBox(composition::Element content, composition::Element label)
    : Element(std::make_unique<Content>(
          components::Group::is_empty(label)
              ? nullptr
              : std::make_shared<composition::PlaceholderContentView::Source>(
                    std::move(label)),
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(content)))) {}

auto GroupBox::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto GroupBox::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const GroupBoxStyle& style = context.environment(GroupBoxStyle::key);
  std::vector<composition::Element> produced;
  produced.push_back(style.make_body(GroupBoxStyle::Configuration{
      .label = label_ != nullptr
                   ? composition::Element{composition::PlaceholderContentView(
                         label_)}
                   : composition::Element{components::Group()},
      .content = composition::Element{
          composition::PlaceholderContentView(content_)}}));
  return produced;
}

}
