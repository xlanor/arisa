#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/structure/disclosure_row.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/space.hpp>

#include <utility>
#include <vector>

namespace arisa::structure {

DisclosureRow::DisclosureRow(std::string title, Action action)
    : DisclosureRow(std::move(action), components::Text(std::move(title))) {}

DisclosureRow::DisclosureRow(std::string title, std::string detail,
                             Action action)
    : DisclosureRow(std::move(action), components::Text(std::move(title)),
                    components::Text(std::move(detail))) {}

DisclosureRow::DisclosureRow(Action action, composition::Element label,
                             std::optional<composition::Element> detail)
    : Button(std::move(action), row(std::move(label), std::move(detail))) {
  if (const auto content = Button::content(*this)) {
    content->get().set_discloses(true);
  }
}

auto DisclosureRow::row(composition::Element label,
                        std::optional<composition::Element> detail)
    -> composition::Element {
  std::vector<composition::Element> parts;
  parts.push_back(std::move(label));
  parts.emplace_back(components::Spacer());
  if (detail.has_value()) {
    parts.push_back(std::move(*detail)
                        .foreground_style(theme::ColourRole::text_secondary)
                        .line_limit(1));
  }
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center,
                 .spacing = theme::space::sm},
             std::move(parts))
      .frame({.max_width = composition::infinity});
}

}
