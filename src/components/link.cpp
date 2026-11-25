#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/link.hpp>
#include <arisa/components/text.hpp>
#include <arisa/text/localized_string_key.hpp>

#include <utility>

namespace arisa::components {

Link::Link(std::string title, std::string destination)
    : destination_(std::move(destination)), title_(std::move(title)) {}

Link::Link(std::string destination, composition::Element label)
    : destination_(std::move(destination)),
      label_(std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(label))) {}

auto Link::body(composition::Context& context) const -> composition::Element {
  const OpenURLAction& open = context.environment(Environment::open_url);
  composition::Element label =
      title_.has_value()
          ? composition::Element{Text(text::LocalizedStringKey{*title_})}
          : composition::Element{composition::PlaceholderContentView(label_)};
  return Button(
             [open, url = destination_] {
               static_cast<void>(open(std::string_view{url}));
             },
             std::move(label))
      .button_style(ButtonStyle::plain())
      .foreground_style(theme::ColourRole::link);
}

}
