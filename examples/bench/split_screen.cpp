#include <split_screen.hpp>

#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <materials_page.hpp>

#include <format>
#include <utility>
#include <vector>

namespace arisa::examples {

auto SplitScreen::body(composition::Context& context) const
    -> composition::Element {
  const auto selection = context.state(selected);
  remote_->selection = selection;
  std::vector<composition::Element> items;
  items.reserve(rows);
  for (int row = 0; row < rows; ++row) {
    items.push_back(components::Label(std::format("Material {}", row),
                                      components::Image::system_name("layers"))
                        .tag(row));
  }
  return composition::Element{
      structure::NavigationSplitView(
          structure::List(selection,
                          structure::Section(structure::Section::Labels{},
                                             std::move(items))),
          Detail{selection.get(), remote_})}
      .list_style(structure::ListStyle::sidebar());
}

auto SplitScreen::Detail::body(composition::Context&) const
    -> composition::Element {
  ++remote_->detail_builds;
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             components::Text(std::format("Material {}", row_))
                 .font(theme::Font::title2),
             composition::Element{MaterialsPage{}}.id(row_))
      .padding(theme::Insets{.top = theme::space::lg,
                             .leading = theme::space::xl,
                             .bottom = theme::space::md,
                             .trailing = theme::space::xl})
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity,
              .alignment = composition::Alignment::leading});
}

}
