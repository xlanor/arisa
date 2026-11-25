#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/gesture.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/inspector/inspector_style.hpp>
#include <arisa/inspector/timeline_panel.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace arisa::inspector {

const std::array<TimelinePanel::Phase, 8> TimelinePanel::cpu_phases{
    Phase{"Evaluate", theme::ColourRole::blue,
          [](const runtime::FrameTimings& frame) { return frame.reconcile; }},
    Phase{"Layout", theme::ColourRole::green,
          [](const runtime::FrameTimings& frame) { return frame.layout; }},
    Phase{"Input", theme::ColourRole::orange,
          [](const runtime::FrameTimings& frame) { return frame.input; }},
    Phase{"Walk", theme::ColourRole::purple,
          [](const runtime::FrameTimings& frame) { return frame.walk; }},
    Phase{"Upload", theme::ColourRole::pink,
          [](const runtime::FrameTimings& frame) { return frame.upload; }},
    Phase{"Emit", theme::ColourRole::teal,
          [](const runtime::FrameTimings& frame) { return frame.emit; }},
    Phase{
        "Present", theme::ColourRole::yellow,
        [](const runtime::FrameTimings& frame) { return frame.present_cpu(); }},
    Phase{"Blocked", theme::ColourRole::gray,
          [](const runtime::FrameTimings& frame) {
            return std::min(frame.blocked(), frame.present);
          }}};

const std::array<TimelinePanel::Phase, 2> TimelinePanel::render_phases{
    Phase{"Record", theme::ColourRole::cyan,
          [](const runtime::FrameTimings& frame) { return frame.record; }},
    Phase{"Wait", theme::ColourRole::gray3,
          [](const runtime::FrameTimings& frame) { return frame.wait; }}};

const std::array<TimelinePanel::Phase, 3> TimelinePanel::gpu_phases{
    Phase{"GPU effects", theme::ColourRole::indigo,
          [](const runtime::FrameTimings& frame) {
            return frame.effects_gpu.value_or(std::chrono::nanoseconds{0});
          }},
    Phase{"GPU UI", theme::ColourRole::mint,
          [](const runtime::FrameTimings& frame) {
            return frame.interface_gpu.value_or(std::chrono::nanoseconds{0});
          }},
    Phase{"GPU hooks", theme::ColourRole::brown,
          [](const runtime::FrameTimings& frame) {
            return frame.hooks_gpu.value_or(std::chrono::nanoseconds{0});
          }}};

template <std::size_t Count>
auto TimelinePanel::chart(const std::array<Phase, Count>& phases,
                          TimelineScale scale, bool marks) const
    -> composition::Element {
  const FrameHistory& history = model_->history();
  const float width = model_->panel_width() - chart_inset;
  const float height = scale.height();
  const std::size_t capacity = history.capacity();
  const float slot = width / static_cast<float>(capacity);
  const float bar = std::max(slot * 0.8F, 0.5F);
  const std::size_t start = capacity - history.size();
  std::vector<composition::Element> layers;
  layers.push_back(
      composition::Element{components::ShapeView(theme::Rectangle{})
                               .fill(theme::ColourRole::quaternary_system_fill)}
          .frame({.width = width, .height = height}));
  const std::optional<std::size_t> chosen = model_->selected_frame();
  if (chosen.has_value()) {
    graphics::Path picked;
    picked.add_rect({.x = static_cast<float>(start + *chosen) * slot,
                     .y = 0.0F,
                     .width = std::max(slot, 2.0F),
                     .height = height});
    layers.push_back(
        composition::Element{components::ShapeView(std::move(picked))
                                 .fill(theme::ColourRole::tertiary_system_fill)}
            .frame({.width = width, .height = height}));
  }
  std::array<graphics::Path, Count> paths;
  graphics::Path capped;
  graphics::Path missed;
  for (std::size_t index = 0; index < history.size(); ++index) {
    const runtime::FrameTimings& frame = history.at(index).timings;
    const float x = static_cast<float>(start + index) * slot;
    std::chrono::nanoseconds total{0};
    for (std::size_t which = 0; which < phases.size(); ++which) {
      const std::chrono::nanoseconds part = phases.at(which).measure(frame);
      const float below = scale.extent(total);
      total += part;
      const float above = scale.extent(total);
      if (part.count() > 0 && above > below) {
        paths.at(which).add_rect({.x = x,
                                  .y = height - above,
                                  .width = bar,
                                  .height = above - below});
      }
    }
    if (scale.clipped(total)) {
      capped.add_rect({.x = x,
                       .y = 0.0F,
                       .width = std::max(slot, 2.0F),
                       .height = marker_height});
    }
    if (marks && frame.missed_vsyncs > 0) {
      missed.add_rect({.x = x,
                       .y = height - marker_height,
                       .width = std::max(slot, 2.0F),
                       .height = marker_height});
    }
  }
  for (std::size_t which = 0; which < phases.size(); ++which) {
    if (paths.at(which).is_empty()) {
      continue;
    }
    layers.push_back(
        composition::Element{components::ShapeView(std::move(paths.at(which)))
                                 .fill(phases.at(which).colour)}
            .frame({.width = width, .height = height}));
  }
  if (marks) {
    graphics::Path line;
    line.add_rect({.x = 0.0F,
                   .y = height - scale.extent(TimelineScale::budget),
                   .width = width,
                   .height = 1.0F});
    layers.push_back(
        composition::Element{components::ShapeView(std::move(line))
                                 .fill(theme::ColourRole::separator)}
            .frame({.width = width, .height = height}));
  }
  if (!capped.is_empty()) {
    layers.push_back(composition::Element{
        components::ShapeView(std::move(capped)).fill(cap_colour)}
                         .frame({.width = width, .height = height}));
  }
  if (!missed.is_empty()) {
    layers.push_back(composition::Element{
        components::ShapeView(std::move(missed)).fill(missed_colour)}
                         .frame({.width = width, .height = height}));
  }
  std::shared_ptr<InspectorModel> model = model_;
  return composition::Element{
      components::ZStack(
          components::ZStackOptions{.alignment =
                                        composition::Alignment::top_leading},
          std::move(layers))}
      .frame({.width = width, .height = height})
      .gesture(composition::DragGesture{0.0F}.on_ended(
          [model, slot, start](const composition::DragGesture::Value& value) {
            const FrameHistory& frames = model->history();
            if (frames.empty() || slot <= 0.0F) {
              return;
            }
            const auto column = static_cast<std::ptrdiff_t>(
                                    std::floor(value.location.x / slot)) -
                                static_cast<std::ptrdiff_t>(start);
            const auto index =
                static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(
                    column, 0, static_cast<std::ptrdiff_t>(frames.size() - 1)));
            model->select_frame(frames.at(index).number);
          }));
}

auto TimelinePanel::swatch(std::string_view label, theme::ColourRole colour)
    -> composition::Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::xxs},
      composition::Element{
          components::ShapeView(theme::RoundedRectangle{2.0F}).fill(colour)}
          .frame({.width = 10.0F, .height = 10.0F}),
      components::Text(std::string{label}).font(theme::Font::caption));
}

auto TimelinePanel::legend() -> composition::Element {
  std::vector<composition::Element> swatches;
  for (const Phase& phase : cpu_phases) {
    swatches.push_back(swatch(phase.label, phase.colour));
  }
  for (const Phase& phase : render_phases) {
    swatches.push_back(swatch(phase.label, phase.colour));
  }
  for (const Phase& phase : gpu_phases) {
    swatches.push_back(swatch(phase.label, phase.colour));
  }
  swatches.push_back(swatch("Over 33 ms", cap_colour));
  swatches.push_back(swatch("Missed vsync", missed_colour));
  return composition::Element{
      components::HStack(
          components::HStackOptions{.spacing = theme::space::sm, .wrap = true},
          std::move(swatches))}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

auto TimelinePanel::controls() const -> composition::Element {
  std::shared_ptr<InspectorModel> model = model_;
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::xs},
      InspectorStyle::tab("Previous", false,
                          [model] { model->step_frame(-1); }),
      InspectorStyle::tab("Next", false, [model] { model->step_frame(1); }),
      InspectorStyle::tab("Latest", false,
                          [model] { model->select_frame(std::nullopt); }));
}

auto TimelinePanel::selection() const -> composition::Element {
  const std::optional<std::size_t> chosen = model_->selected_frame();
  if (!chosen.has_value()) {
    return InspectorStyle::line("No frames");
  }
  std::vector<composition::Element> rows;
  rows.push_back(InspectorStyle::line(model_->breakdown(*chosen)));
  for (const InspectorModel::Counter& counter : model_->counters(*chosen)) {
    rows.push_back(InspectorStyle::value(counter.first, counter.second));
  }
  return InspectorStyle::section("Frame", std::move(rows));
}

auto TimelinePanel::body(composition::Context& context) const
    -> composition::Element {
  static_cast<void>(context.read(model_->state()));
  static_cast<void>(context.read(model_->frames()));
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::sm},
          chart(cpu_phases, TimelineScale::cpu(cpu_height), true),
          chart(render_phases, TimelineScale::gpu(gpu_height), false),
          chart(gpu_phases, TimelineScale::gpu(gpu_height), false), legend(),
          controls(), selection())}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::top_leading});
}

}
