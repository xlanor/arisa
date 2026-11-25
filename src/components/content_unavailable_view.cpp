#include <arisa/components/content_unavailable_view.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <utility>

namespace arisa::components {

ContentUnavailableView::ContentUnavailableView(std::string title,
                                               std::string system_image)
    : Element(std::make_unique<Content>(
          Content::Parts{.title = std::move(title),
                         .symbol = std::move(system_image),
                         .label = nullptr,
                         .description = nullptr,
                         .actions = nullptr})) {}

ContentUnavailableView::ContentUnavailableView(std::string title,
                                               std::string system_image,
                                               composition::Element description)
    : Element(std::make_unique<Content>(
          Content::Parts{.title = std::move(title),
                         .symbol = std::move(system_image),
                         .label = nullptr,
                         .description = Content::source(std::move(description)),
                         .actions = nullptr})) {}

ContentUnavailableView::ContentUnavailableView(
    std::string title, Image::Resource image,
    std::optional<composition::Element> description)
    : ContentUnavailableView(Label(std::move(title), std::move(image)),
                             std::move(description), std::nullopt) {}

ContentUnavailableView::ContentUnavailableView(
    composition::Element label, std::optional<composition::Element> description,
    std::optional<composition::Element> actions)
    : Element(std::make_unique<Content>(
          Content::Parts{.title = std::nullopt,
                         .symbol = std::nullopt,
                         .label = Content::source(std::move(label)),
                         .description = Content::source(std::move(description)),
                         .actions = Content::source(std::move(actions))})) {}

auto ContentUnavailableView::search() -> ContentUnavailableView {
  return ContentUnavailableView{"No Results", std::string{search_symbol}};
}

auto ContentUnavailableView::search(std::string text)
    -> ContentUnavailableView {
  if (text.empty()) {
    return search();
  }
  return ContentUnavailableView{std::format("No Results for “{}”", text),
                                std::string{search_symbol}};
}

auto ContentUnavailableView::Content::source(
    std::optional<composition::Element> element) -> Source {
  if (!element.has_value()) {
    return nullptr;
  }
  return std::make_shared<composition::PlaceholderContentView::Source>(
      std::move(*element));
}

auto ContentUnavailableView::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ContentUnavailableView::Content::same_as(
    const composition::ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  const Parts& theirs = static_cast<const Content&>(other).parts_;
  return parts_.title == theirs.title && parts_.symbol == theirs.symbol &&
         (parts_.label == nullptr) == (theirs.label == nullptr) &&
         (parts_.description == nullptr) == (theirs.description == nullptr) &&
         (parts_.actions == nullptr) == (theirs.actions == nullptr);
}

auto ContentUnavailableView::Content::adopt(
    composition::ElementContent& incoming) -> bool {
  Parts& newer = static_cast<Content&>(incoming).parts_;
  bool forwarded = false;
  for (const auto& [held, arriving] :
       {std::pair{parts_.label, newer.label},
        std::pair{parts_.description, newer.description},
        std::pair{parts_.actions, newer.actions}}) {
    if (held != nullptr && arriving != nullptr) {
      held->forward(*arriving);
      forwarded = true;
    }
  }
  return forwarded;
}

auto ContentUnavailableView::StackedLabelStyle::make_body(
    LabelStyle::Configuration configuration) const -> composition::Element {
  return VStack(VStackOptions{.alignment = HorizontalAlignment::center,
                              .spacing = label_spacing},
                std::move(configuration.icon)
                    .font(theme::Font::system(icon_size))
                    .foreground_style(theme::ColourRole::tertiary_label)
                    .frame({.height = icon_height}),
                std::move(configuration.title).font(title_));
}

auto ContentUnavailableView::Content::label(composition::Context& context) const
    -> composition::Element {
  if (parts_.label != nullptr) {
    const theme::TypeStyle title =
        context.environment(Environment::theme).type(theme::TextRole::title2);
    return composition::Element{
        composition::PlaceholderContentView(parts_.label)}
        .label_style(StackedLabelStyle{
            theme::Font::system(title.size, theme::Font::Weight::bold)})
        .font(theme::Font::title2)
        .foreground_style(theme::ColourRole::secondary_label);
  }
  return VStack(
      VStackOptions{.alignment = HorizontalAlignment::center,
                    .spacing = label_spacing},
      composition::Element{Image::system_name(parts_.symbol.value_or(""))
                               .font(theme::Font::system(icon_size))}
          .foreground_style(theme::ColourRole::tertiary_label)
          .frame({.height = icon_height}),
      Text(parts_.title.value_or(""))
          .font(theme::Font::title2)
          .font_weight(theme::Font::Weight::bold)
          .foreground_style(theme::ColourRole::secondary_label));
}

auto ContentUnavailableView::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> parts;
  parts.push_back(label(context));
  if (parts_.description != nullptr) {
    parts.push_back(composition::Element{
        composition::PlaceholderContentView(parts_.description)}
                        .font(theme::Font::title3)
                        .foreground_style(theme::ColourRole::secondary_label)
                        .padding(theme::Insets{.top = description_spacing}));
  }
  if (parts_.actions != nullptr) {
    parts.push_back(composition::Element{
        composition::PlaceholderContentView(parts_.actions)}
                        .padding(theme::Insets{.top = actions_spacing}));
  }
  std::vector<composition::Element> produced;
  produced.push_back(
      VStack(VStackOptions{.alignment = HorizontalAlignment::center,
                           .spacing = 0.0F},
             std::move(parts))
          .multiline_text_alignment(composition::TextAlignment::center)
          .padding(theme::Insets::all(insets))
          .frame({.max_width = composition::infinity}));
  return produced;
}

}
