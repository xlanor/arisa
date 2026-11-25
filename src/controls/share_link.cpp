#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/pasteboard.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/controls/share_link.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <memory>
#include <utility>

namespace arisa::controls {

using components::Button;
using components::ButtonStyle;
using components::Divider;
using components::HorizontalAlignment;
using components::HStack;
using components::HStackOptions;
using components::Image;
using components::Label;
using components::Text;
using components::VerticalAlignment;
using components::VStack;
using components::VStackOptions;

ShareLink::ShareLink(std::string title,
                     std::vector<composition::TransferItem> items,
                     Options options)
    : ShareLink(
          std::move(items), std::move(options),
          Label(std::move(title), Image::SystemName{std::string{symbol}})) {}

ShareLink::ShareLink(std::vector<composition::TransferItem> items,
                     Options options, composition::Element label)
    : Element(std::make_unique<Content>(
          std::move(items), std::move(options),
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(label)))) {}

auto ShareLink::panel(const ShareDestination::Request& request,
                      const std::optional<SharePreview>& preview,
                      const std::vector<ShareDestination>& destinations,
                      std::function<void()> close) -> composition::Element {
  std::vector<composition::Element> rows;
  if (preview.has_value()) {
    std::vector<composition::Element> header;
    if (std::optional<Image> icon = preview->icon()) {
      header.push_back(composition::Element{std::move(*icon)});
    } else if (std::optional<Image> image = preview->image()) {
      header.push_back(std::move(*image).resizable().scaled_to_fit().frame(
          {.width = 40.0F, .height = 40.0F}));
    }
    header.push_back(Text(preview->title()).font(theme::Font::headline));
    rows.push_back(HStack(HStackOptions{.alignment = VerticalAlignment::center,
                                        .spacing = theme::space::sm},
                          std::move(header)));
  }
  if (request.subject.has_value()) {
    rows.push_back(Text(*request.subject).font(theme::Font::subheadline));
  }
  if (request.message.has_value()) {
    rows.push_back(Text(*request.message)
                       .font(theme::Font::footnote)
                       .foreground_style(theme::ColourRole::text_secondary));
  }
  if (!rows.empty()) {
    rows.push_back(Divider());
  }
  const auto shared =
      std::make_shared<const ShareDestination::Request>(request);
  for (const ShareDestination& destination : destinations) {
    rows.push_back(Button(destination.title(), destination.symbol(),
                          [destination, shared, close] {
                            destination.share(*shared);
                            if (close) {
                              close();
                            }
                          })
                       .button_style(ButtonStyle::borderless())
                       .frame({.max_width = composition::infinity,
                               .alignment = composition::Alignment::leading}));
  }
  return VStack(VStackOptions{.alignment = HorizontalAlignment::leading,
                              .spacing = theme::space::sm},
                std::move(rows))
      .padding(theme::space::md)
      .frame({.width = panel_width})
      .id(std::string{panel_key});
}

auto ShareLink::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ShareLink::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto showing = context.state(presented);
  std::vector<ShareDestination> destinations;
  if (const std::optional<composition::Pasteboard>& board =
          context.environment(composition::Pasteboard::general)) {
    destinations.emplace_back(
        std::string{copy_title}, Image::SystemName{std::string{copy_symbol}},
        [pasteboard = *board](const ShareDestination::Request& request) {
          pasteboard.set_items(request.items);
        });
  }
  for (const ShareDestination& added :
       context.environment(ShareDestination::key)) {
    destinations.push_back(added);
  }
  ShareDestination::Request request{.items = items_,
                                    .subject = options_.subject,
                                    .message = options_.message};
  std::vector<composition::Element> produced;
  produced.push_back(
      Button([showing] { showing.set(true); },
             composition::PlaceholderContentView(label_))
          .popover(showing, composition::PopoverAttachmentAnchor::rect(),
                   theme::Edge::top,
                   [request = std::move(request), preview = options_.preview,
                    destinations = std::move(destinations), showing] {
                     return panel(request, preview, destinations,
                                  [showing] { showing.set(false); });
                   }));
  return produced;
}

}
