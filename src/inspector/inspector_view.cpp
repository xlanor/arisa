#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/inspector/cost_panel.hpp>
#include <arisa/inspector/focus_panel.hpp>
#include <arisa/inspector/gpu_panel.hpp>
#include <arisa/inspector/inspector_model.hpp>
#include <arisa/inspector/inspector_style.hpp>
#include <arisa/inspector/inspector_view.hpp>
#include <arisa/inspector/pick_panel.hpp>
#include <arisa/inspector/timeline_panel.hpp>
#include <arisa/inspector/tokens_panel.hpp>
#include <arisa/inspector/tree_panel.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/shape.hpp>

#include <string>
#include <utility>
#include <vector>

namespace arisa::inspector {

auto InspectorView::outline(const composition::Rect& bounds,
                            theme::ColourRole colour, std::string_view key)
    -> composition::Element {
  return composition::Element{
      components::ShapeView(theme::Rectangle{})
          .stroke(colour, InspectorStyle::outline_width)}
      .frame({.width = bounds.width, .height = bounds.height})
      .offset(bounds.x, bounds.y)
      .allows_hit_testing(false)
      .id(std::string{key});
}

auto InspectorView::cursor(composition::Point at) -> composition::Element {
  return composition::Element{
      components::ShapeView(theme::Circle{})
          .stroke(InspectorStyle::hover, InspectorStyle::outline_width)}
      .frame({.width = cursor_size, .height = cursor_size})
      .offset(at.x - (cursor_size * 0.5F), at.y - (cursor_size * 0.5F))
      .allows_hit_testing(false)
      .id(std::string{cursor_key});
}

auto InspectorView::scrolled(composition::Element panel)
    -> composition::Element {
  return components::ScrollView(std::move(panel).frame(
      {.max_width = composition::infinity,
       .alignment = composition::Alignment::top_leading}));
}

auto InspectorView::header() const -> composition::Element {
  std::vector<composition::Element> tabs;
  for (const InspectorPanel each : InspectorPanels::all) {
    tabs.push_back(InspectorStyle::tab(
        InspectorPanels::name(each), model_->panel() == each,
        [model = model_, each] { model->show(each); }));
  }
  const bool paused = model_->paused();
  return composition::Element{
      components::HStack(
          components::HStackOptions{.spacing = theme::space::xs},
          components::HStack(
              components::HStackOptions{.spacing = theme::space::xxs},
              std::move(tabs)),
          components::Spacer(),
          InspectorStyle::icon(
              paused ? "Resume" : "Pause", paused ? "play_arrow" : "pause",
              paused, [model = model_, paused] { model->set_paused(!paused); }),
          InspectorStyle::icon("Close", "close", false,
                               [model = model_] { model->close(); }))}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

auto InspectorView::content() const -> composition::Element {
  switch (model_->panel()) {
    case InspectorPanel::tree:
      return TreePanel{model_};
    case InspectorPanel::pick:
      return PickPanel{model_};
    case InspectorPanel::focus:
      return scrolled(FocusPanel{model_});
    case InspectorPanel::timeline:
      return scrolled(TimelinePanel{model_});
    case InspectorPanel::cost:
      return scrolled(CostPanel{model_});
    case InspectorPanel::gpu:
      return scrolled(GpuPanel{model_});
    case InspectorPanel::tokens:
      return TokensPanel{model_};
  }
  return TreePanel{model_};
}

auto InspectorView::panel() const -> composition::Element {
  const bool compact = model_->panel() == InspectorPanel::pick;
  composition::Element stacked{components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::md},
      header(),
      content().frame(
          {.max_width = composition::infinity,
           .max_height =
               compact
                   ? std::optional<composition::Length>{}
                   : std::optional<composition::Length>{composition::infinity},
           .alignment = composition::Alignment::top_leading}))};
  return std::move(stacked)
      .padding(theme::space::md)
      .frame(
          {.width = model_->panel_width(),
           .max_height =
               compact
                   ? std::optional<composition::Length>{}
                   : std::optional<composition::Length>{composition::infinity},
           .alignment = composition::Alignment::top_leading})
      .background(theme::ColourRole::secondary_system_background,
                  theme::RoundedRectangle{corner_radius})
      .border(theme::ColourRole::separator)
      .shadow(theme::Elevation::medium)
      .id(std::string{panel_key})
      .margin(theme::space::sm);
}

auto InspectorView::body(composition::Context& context) const
    -> composition::Element {
  static_cast<void>(context.read(model_->state()));
  if (model_->selected().has_value() ||
      model_->panel() == InspectorPanel::pick) {
    static_cast<void>(context.read(model_->frames()));
  }
  std::vector<composition::Element> layers;
  if (const auto selected = model_->selected()) {
    if (const auto bounds = model_->bounds(*selected)) {
      layers.push_back(outline(*bounds, InspectorStyle::outline, outline_key));
    }
  }
  if (model_->panel() == InspectorPanel::pick) {
    if (const auto hovered = model_->hovered()) {
      if (const auto bounds = model_->bounds(*hovered)) {
        layers.push_back(outline(*bounds, InspectorStyle::hover, hover_key));
      }
    }
    layers.push_back(cursor(model_->cursor()));
  }
  layers.push_back(
      composition::Element{
          components::HStack(
              components::HStackOptions{.alignment =
                                            components::VerticalAlignment::top},
              components::Spacer(), panel())}
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::top_trailing}));
  return composition::Element{
      components::ZStack(
          components::ZStackOptions{.alignment =
                                        composition::Alignment::top_leading},
          std::move(layers))}
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity,
              .alignment = composition::Alignment::top_leading});
}

}
