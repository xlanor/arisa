#include <arisa/components/vstack.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/inspector/focus_panel.hpp>
#include <arisa/inspector/inspector_style.hpp>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::inspector {

auto FocusPanel::body(composition::Context& context) const
    -> composition::Element {
  static_cast<void>(context.read(model_->state()));
  const runtime::Introspection::FocusRecord& focus = model_->focus_snapshot();
  const auto name = [this](std::optional<composition::NodeId> node) {
    return node.has_value() ? model_->describe(*node) : std::string{"none"};
  };
  std::vector<composition::Element> sections;
  std::vector<composition::Element> focused;
  focused.push_back(InspectorStyle::line(name(focus.focused)));
  sections.push_back(InspectorStyle::section("Focused", std::move(focused)));
  std::vector<composition::Element> chain;
  for (const composition::NodeId id : focus.chain) {
    chain.push_back(InspectorStyle::line(model_->describe(id)));
  }
  if (chain.empty()) {
    chain.push_back(InspectorStyle::line("none"));
  }
  sections.push_back(InspectorStyle::section("Chain", std::move(chain)));
  std::vector<composition::Element> scopes;
  for (const composition::NodeId id : focus.scopes) {
    scopes.push_back(InspectorStyle::line(model_->describe(id)));
  }
  if (scopes.empty()) {
    scopes.push_back(InspectorStyle::line("none"));
  }
  sections.push_back(InspectorStyle::section("Scopes", std::move(scopes)));
  constexpr std::array<std::string_view, 4> labels{"Up", "Down", "Left",
                                                   "Right"};
  std::vector<composition::Element> next;
  for (std::size_t index = 0; index < labels.size(); ++index) {
    next.push_back(
        InspectorStyle::value(labels.at(index), name(focus.next.at(index))));
  }
  sections.push_back(InspectorStyle::section("Next", std::move(next)));
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::md},
          std::move(sections))}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::top_leading});
}

}
