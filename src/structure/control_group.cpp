#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/structure/control_group.hpp>
#include <arisa/structure/control_group_style.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace arisa::structure {

ControlGroup::ControlGroup(Builder content)
    : ControlGroup(std::move(content), components::Group()) {}

ControlGroup::ControlGroup(std::string title, Builder content)
    : ControlGroup(
          std::move(content),
          components::Text(text::LocalizedStringKey{std::move(title)})) {}

ControlGroup::ControlGroup(Builder content, composition::Element label)
    : Element(std::make_unique<Content>(
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(label)),
          std::move(content))) {}

auto ControlGroup::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ControlGroup::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const ControlGroupStyle& style = context.environment(ControlGroupStyle::key);
  std::vector<composition::Element> produced;
  produced.push_back(style.make_body(ControlGroupStyle::Configuration{
      .label =
          composition::Element{composition::PlaceholderContentView(label_)},
      .content = content_}));
  return produced;
}

}
