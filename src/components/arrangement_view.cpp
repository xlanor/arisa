#include <arisa/components/arrangement_view.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/user_interface_size_class.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

ArrangementView::ArrangementView(composition::Element primary,
                                 composition::Element secondary)
    : ArrangementView(std::move(primary), std::move(secondary), false) {}

ArrangementView::ArrangementView(
    ArrangementViewStyle::Configuration configuration)
    : ArrangementView(std::move(configuration.primary),
                      std::move(configuration.secondary), true) {}

ArrangementView::ArrangementView(composition::Element primary,
                                 composition::Element secondary,
                                 bool configured)
    : Element(std::make_unique<Content>(
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(primary)),
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(secondary)),
          configured)) {}

auto ArrangementView::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ArrangementView::Content::resolved(composition::Context& context) const
    -> ArrangementViewStyle {
  ArrangementViewStyle style = context.environment(ArrangementViewStyle::key);
  if (style.kind() == ArrangementViewStyle::Kind::custom && configured_) {
    style = ArrangementViewStyle::automatic();
  }
  if (style.kind() != ArrangementViewStyle::Kind::automatic) {
    return style;
  }
  const bool compact =
      context.environment(Environment::horizontal_size_class) ==
      UserInterfaceSizeClass::compact;
  if (compact) {
    return ArrangementViewStyle::overlay();
  }
  return ArrangementViewStyle::split();
}

auto ArrangementView::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  produced.push_back(
      resolved(context).make_body(ArrangementViewStyle::Configuration{
          .primary = composition::Element{composition::PlaceholderContentView(
              primary_)},
          .secondary = composition::Element{
              composition::PlaceholderContentView(secondary_)}}));
  return produced;
}

}
