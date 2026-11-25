#include <arisa/presentation/inspector.hpp>
#include <arisa/presentation/presentations.hpp>

#include <arisa/components/column_resizer.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/inspector_column_width.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/space.hpp>

#include <optional>
#include <string>

namespace arisa::presentation {

using composition::Element;
using composition::infinity;

Inspector::Inspector(Element content, composition::Binding<bool> is_presented,
                     Builder build)
    : Element(std::make_unique<Content>(nullptr, std::move(is_presented),
                                        std::move(build))) {
  inherit_identity(*this, content);
  inherit(*this, content);
  edit<Content>().hold(
      std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(content)));
}

auto Inspector::column(Element content, composition::Binding<bool> is_presented)
    -> Element {
  const auto& declared = content.interactions().presentation;
  const float width =
      declared != nullptr && declared->inspector_width.has_value()
          ? declared->inspector_width->resolved()
          : composition::InspectorColumnWidth{}.resolved();
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             std::move(content))
      .padding(theme::space::lg)
      .frame({.width = width,
              .max_height = infinity,
              .alignment = composition::Alignment::top_leading})
      .background(theme::ColourRole::surface)
      .overlay(composition::Alignment::leading,
               components::ZStack()
                   .frame({.width = divider_width, .max_height = infinity})
                   .background(theme::ColourRole::separator))
      .focus_section()
      .on_exit_command(std::string{close_label},
                       [is_presented] { is_presented.set(false); })
      .transition(motion::Transition::move(theme::Edge::trailing));
}

auto Inspector::column(Element content, composition::Binding<bool> is_presented,
                       composition::StateHandle<std::optional<float>> width,
                       composition::StateHandle<std::optional<float>> held)
    -> Element {
  const auto& declared = content.interactions().presentation;
  const composition::InspectorColumnWidth measure =
      declared != nullptr && declared->inspector_width.has_value()
          ? *declared->inspector_width
          : composition::InspectorColumnWidth{};
  if (!measure.resizable()) {
    return column(std::move(content), std::move(is_presented));
  }
  const float chosen = width.get().has_value() ? measure.clamped(*width.get())
                                               : measure.resolved();
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             std::move(content))
      .padding(theme::space::lg)
      .frame({.width = chosen,
              .max_height = infinity,
              .alignment = composition::Alignment::top_leading})
      .background(theme::ColourRole::surface)
      .overlay(composition::Alignment::leading,
               components::ZStack()
                   .frame({.width = divider_width, .max_height = infinity})
                   .background(theme::ColourRole::separator))
      .overlay(composition::Alignment::leading,
               components::ColumnResizer{
                   components::ColumnResizer::Resize{.width = width,
                                                     .anchor = held,
                                                     .current = chosen,
                                                     .minimum = measure.lower(),
                                                     .maximum = measure.upper(),
                                                     .direction = -1.0F}})
      .focus_section()
      .on_exit_command(std::string{close_label},
                       [is_presented] { is_presented.set(false); })
      .transition(motion::Transition::move(theme::Edge::trailing));
}

auto Inspector::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Inspector::Content::body(composition::Context& context) const
    -> std::vector<Element> {
  const bool compact =
      context.environment(components::Environment::horizontal_size_class) ==
      components::UserInterfaceSizeClass::compact;
  const bool presented = is_presented_.get();
  std::optional<Element> side;
  if (presented && !compact) {
    side = Inspector::column(build_(), is_presented_, context.state(dragged),
                             context.state(anchor));
  }
  const composition::Binding<bool> bound = is_presented_;
  composition::Binding<bool> as_sheet{
      [bound, compact] { return compact && bound.get(); },
      [bound](bool shown) { bound.set(shown); }};
  std::vector<Element> produced;
  produced.push_back(
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::top,
              .spacing = theme::space::none},
          Element{composition::PlaceholderContentView(presenter_)}.frame(
              {.max_width = infinity, .max_height = infinity}),
          std::move(side))
          .frame({.max_width = infinity, .max_height = infinity})
          .animation(motion::Animation::smooth(0.35), presented && !compact)
          .sheet(std::move(as_sheet), [build = build_] { return build(); }));
  return produced;
}

}
