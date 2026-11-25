#include <arisa/components/label.hpp>
#include <arisa/components/label_style.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/control_environment.hpp>

#include <utility>

namespace arisa::components {

Label::Label(std::string title, composition::Element icon)
    : Element(std::make_unique<Content>(
          std::move(title), nullptr,
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(icon)))) {}

Label::Label(composition::Element title, composition::Element icon)
    : Element(std::make_unique<Content>(
          std::nullopt,
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(title)),
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(icon)))) {}

Label::Label(std::string title, Image::SystemName system_image)
    : Label(std::move(title), Image{std::move(system_image)}) {}

Label::Label(std::string title, Image::Resource image)
    : Label(std::move(title), Image{std::move(image)}) {}

auto Label::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Label::Content::same_as(const composition::ElementContent& other) const
    -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return title_ == theirs.title_ &&
         (written_ == nullptr) == (theirs.written_ == nullptr) &&
         (icon_ == nullptr) == (theirs.icon_ == nullptr);
}

auto Label::Content::adopt(composition::ElementContent& incoming) -> bool {
  auto& newer = static_cast<Content&>(incoming);
  bool forwarded = false;
  if (written_ != nullptr && newer.written_ != nullptr) {
    written_->forward(*newer.written_);
    forwarded = true;
  }
  if (icon_ != nullptr && newer.icon_ != nullptr) {
    icon_->forward(*newer.icon_);
    forwarded = true;
  }
  return forwarded;
}

auto Label::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const LabelStyle& style = context.environment(LabelStyle::key);
  composition::Element title =
      title_.has_value()
          ? composition::Element{Text(text::LocalizedStringKey{*title_})}
          : composition::Element{composition::PlaceholderContentView(written_)};
  std::vector<composition::Element> produced;
  produced.push_back(style.make_body(LabelStyle::Configuration{
      .icon = composition::PlaceholderContentView(icon_),
      .title = std::move(title),
      .icon_to_title_spacing = context.environment(
          composition::ControlEnvironment::label_icon_to_title_spacing),
      .reserved_icon_width = context.environment(
          composition::ControlEnvironment::label_reserved_icon_width)}));
  return produced;
}

}
