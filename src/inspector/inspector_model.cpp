#include <arisa/composition/element_key.hpp>
#include <arisa/inspector/inspector_model.hpp>
#include <arisa/inspector/inspector_view.hpp>
#include <arisa/runtime/frame_breakdown.hpp>

#include <algorithm>
#include <format>
#include <ranges>
#include <utility>

namespace arisa::inspector {

InspectorModel::InspectorModel(runtime::Application& application,
                               InspectorOptions options)
    : application_(application), options_(options), history_(options.history) {}

auto InspectorModel::introspection() const -> const runtime::Introspection& {
  return application_.get().introspection();
}

auto InspectorModel::panel_width() const -> float {
  const float available = introspection().viewport().width * panel_share;
  return available > 0.0F ? std::min(options_.panel_width, available)
                          : options_.panel_width;
}

void InspectorModel::bump(composition::Observable<std::uint64_t>& observable) {
  observable.set(observable.get() + 1);
}

auto InspectorModel::held(const provider::InputFrame& input,
                          provider::Buttons buttons) noexcept -> bool {
  if (buttons.empty()) {
    return false;
  }
  return std::ranges::any_of(
      input.pads, [buttons](const provider::PadState& pad) {
        return (pad.buttons.bits() & buttons.bits()) == buttons.bits();
      });
}

auto InspectorModel::pressed(const provider::InputFrame& input) noexcept
    -> provider::Buttons {
  provider::Buttons all;
  for (const provider::PadState& pad : input.pads) {
    all = all | pad.buttons;
  }
  return all;
}

void InspectorModel::strip(provider::InputFrame& input,
                           provider::Buttons buttons) {
  for (provider::PadState& pad : input.pads) {
    for (const provider::Button button : provider::all_buttons) {
      if (buttons.contains(button)) {
        pad.buttons = pad.buttons.without(button);
      }
    }
  }
}

auto InspectorModel::toggled(provider::InputFrame& input) -> bool {
  bool toggle = false;
  const bool chord = held(input, options_.chord);
  if (chord) {
    toggle = !chord_held_;
    strip(input, options_.chord);
  }
  chord_held_ = chord;
  const provider::Key key = options_.key;
  if (std::ranges::any_of(input.keys, [key](const provider::KeyEvent& event) {
        return event.key == key && event.down;
      })) {
    toggle = true;
  }
  std::erase_if(input.keys, [key](const provider::KeyEvent& event) {
    return event.key == key;
  });
  return toggle;
}

void InspectorModel::filter(provider::InputFrame& input) {
  const provider::Buttons previous = previous_;
  previous_ = pressed(input);
  if (toggled(input)) {
    toggle();
  }
  if (open_ && panel_ == InspectorPanel::pick) {
    steer(input, previous);
  }
}

auto InspectorModel::inside_panel(composition::Point point) const -> bool {
  const composition::Tree& tree = application_.get().root().tree();
  const auto found =
      tree.find(composition::ElementKey{std::string{InspectorView::panel_key}});
  if (!found.has_value()) {
    return false;
  }
  const auto node = tree.node(*found);
  if (!node.has_value()) {
    return false;
  }
  const composition::Rect frame = node->get().presented().has_value()
                                      ? node->get().presented()->bounds
                                      : node->get().frame();
  return frame.contains(point.x, point.y);
}

void InspectorModel::steer(provider::InputFrame& input,
                           provider::Buttons previous) {
  using provider::Button;
  const provider::Buttons now = pressed(input);
  const auto edge = [&](Button button) {
    return now.contains(button) && !previous.contains(button);
  };
  composition::Point target = cursor_;
  if (now.contains(Button::dpad_left)) {
    target.x -= cursor_step;
  }
  if (now.contains(Button::dpad_right)) {
    target.x += cursor_step;
  }
  if (now.contains(Button::dpad_up)) {
    target.y -= cursor_step;
  }
  if (now.contains(Button::dpad_down)) {
    target.y += cursor_step;
  }
  if (target.x != cursor_.x || target.y != cursor_.y) {
    move_cursor(target);
  }
  if (edge(Button::a)) {
    static_cast<void>(pick(cursor_));
  } else if (edge(Button::b)) {
    show(InspectorPanel::tree);
  }
  strip(input, provider::Buttons{Button::a, Button::b, Button::dpad_up,
                                 Button::dpad_down, Button::dpad_left,
                                 Button::dpad_right});
  std::optional<composition::Point> touched;
  for (const provider::Touch& touch : input.touches) {
    if (touch.phase == provider::TouchPhase::began) {
      touched = introspection().points(touch.position);
    }
  }
  if (touched.has_value() && inside_panel(*touched)) {
    return;
  }
  input.touches.clear();
  if (touched.has_value()) {
    move_cursor(*touched);
    static_cast<void>(pick(*touched));
  }
}

void InspectorModel::observe(const runtime::FrameTimings& frame) {
  if (!open_ || paused_ || frame.kind == runtime::FrameKind::idle) {
    return;
  }
  const std::vector<runtime::Introspection::CostRecord> costs =
      introspection().costs();
  if (frame.debug) {
    ++own_frames_;
    ledger_.absorb(costs);
    return;
  }
  history_.record(frame, ledger_.advance(costs, options_.ranked));
  bump(frames_);
  if (frame.evaluations > 0 || frame.nodes_created > 0 || frame.laid_out) {
    bump(structure_);
  }
}

void InspectorModel::open() {
  if (open_) {
    return;
  }
  focus_ = introspection().focus();
  const std::vector<runtime::Introspection::CostRecord> costs =
      introspection().costs();
  ledger_.reset(costs);
  history_.clear();
  own_frames_ = 0;
  selected_frame_.reset();
  const composition::Size viewport = introspection().viewport();
  cursor_ = composition::Point{.x = viewport.width * 0.5F,
                               .y = viewport.height * 0.5F};
  open_ = true;
  application_.get().root().tree().present_debug_layer(
      composition::Element{InspectorView{shared_from_this()}});
  bump(state_);
}

void InspectorModel::close() {
  if (!open_) {
    return;
  }
  open_ = false;
  application_.get().root().tree().present_debug_layer(std::nullopt);
  bump(state_);
}

void InspectorModel::toggle() {
  if (open_) {
    close();
  } else {
    open();
  }
}

void InspectorModel::show(InspectorPanel panel) {
  if (panel_ == panel) {
    return;
  }
  panel_ = panel;
  if (panel == InspectorPanel::pick) {
    hovered_ = introspection().hit(cursor_);
  }
  bump(state_);
}

void InspectorModel::select(std::optional<composition::NodeId> node) {
  if (selected_ == node) {
    return;
  }
  selected_ = node;
  if (node.has_value()) {
    reveal(*node);
  }
  bump(state_);
}

void InspectorModel::reveal(composition::NodeId node) {
  for (std::optional<runtime::Introspection::NodeRecord> at =
           introspection().node(node);
       at.has_value() && at->parent.has_value();
       at = introspection().node(*at->parent)) {
    collapsed_.erase(*at->parent);
    expanded_.insert(*at->parent);
  }
}

auto InspectorModel::expanded(composition::NodeId node) const -> bool {
  constexpr std::size_t open_depth = 3;
  if (expanded_.contains(node)) {
    return true;
  }
  if (collapsed_.contains(node)) {
    return false;
  }
  const auto found = introspection().node(node);
  return found.has_value() && found->depth < open_depth;
}

void InspectorModel::toggle_expanded(composition::NodeId node) {
  if (expanded(node)) {
    expanded_.erase(node);
    collapsed_.insert(node);
  } else {
    collapsed_.erase(node);
    expanded_.insert(node);
  }
  bump(state_);
}

void InspectorModel::set_paused(bool paused) {
  if (paused_ == paused) {
    return;
  }
  paused_ = paused;
  if (!paused) {
    ledger_.absorb(introspection().costs());
  }
  bump(state_);
}

auto InspectorModel::pick(composition::Point point)
    -> std::optional<composition::NodeId> {
  const std::optional<composition::NodeId> found = introspection().hit(point);
  if (found.has_value()) {
    select(found);
    show(InspectorPanel::tree);
  }
  return found;
}

void InspectorModel::move_cursor(composition::Point to) {
  const composition::Size viewport = introspection().viewport();
  cursor_ = composition::Point{.x = std::clamp(to.x, 0.0F, viewport.width),
                               .y = std::clamp(to.y, 0.0F, viewport.height)};
  hovered_ = introspection().hit(cursor_);
  bump(state_);
}

void InspectorModel::select_frame(std::optional<std::uint64_t> number) {
  if (selected_frame_ == number) {
    return;
  }
  selected_frame_ = number;
  bump(state_);
}

void InspectorModel::step_frame(int offset) {
  if (history_.empty()) {
    return;
  }
  const std::size_t last = history_.size() - 1;
  const std::size_t current = selected_frame().value_or(last);
  const auto moved = static_cast<std::ptrdiff_t>(current) + offset;
  const auto clamped = static_cast<std::size_t>(
      std::clamp<std::ptrdiff_t>(moved, 0, static_cast<std::ptrdiff_t>(last)));
  select_frame(clamped == last ? std::nullopt
                               : std::optional{history_.at(clamped).number});
}

auto InspectorModel::selected_frame() const -> std::optional<std::size_t> {
  if (history_.empty()) {
    return std::nullopt;
  }
  if (selected_frame_.has_value()) {
    if (const auto index = history_.index_of(*selected_frame_)) {
      return index;
    }
  }
  return history_.size() - 1;
}

void InspectorModel::set_cost_span(CostSpan span) {
  if (cost_span_ == span) {
    return;
  }
  cost_span_ = span;
  bump(state_);
}

auto InspectorModel::label(const runtime::Introspection::NodeRecord& record)
    -> std::string {
  if (record.key.empty()) {
    return record.name;
  }
  return std::format("{}  {}", record.name, record.key);
}

auto InspectorModel::describe(composition::NodeId node) const -> std::string {
  const auto found = introspection().node(node);
  if (!found.has_value()) {
    return std::format("#{}", node.value());
  }
  return label(*found);
}

auto InspectorModel::rows() const -> std::vector<Row> {
  const std::vector<runtime::Introspection::NodeRecord> all =
      introspection().nodes();
  std::vector<Row> found;
  std::flat_set<composition::NodeId> hidden;
  for (const runtime::Introspection::NodeRecord& record : all) {
    if (record.parent.has_value() && hidden.contains(*record.parent)) {
      hidden.insert(record.id);
      continue;
    }
    const bool open = expanded(record.id);
    if (!open) {
      hidden.insert(record.id);
    }
    found.push_back(Row{.record = record,
                        .expandable = record.children > 0,
                        .expanded = open && record.children > 0});
  }
  return found;
}

auto InspectorModel::indentation(const std::vector<Row>& rows) const
    -> Indentation {
  const float content = std::max(panel_width() - row_inset, 0.0F);
  if (rows.empty()) {
    return Indentation{.base = 0, .step = indent_step, .content = content};
  }
  const auto [shallowest, deepest] =
      std::ranges::minmax(rows | std::views::transform([](const Row& row) {
                            return row.record.depth;
                          }));
  const float room = std::max(
      content - disclosure_width - row_spacing - (panel_width() * label_share),
      0.0F);
  const std::size_t span = deepest - shallowest;
  const float step =
      span == 0 ? indent_step
                : std::min(indent_step, room / static_cast<float>(span));
  return Indentation{.base = shallowest, .step = step, .content = content};
}

auto InspectorModel::ranked() const -> std::vector<CostLedger::Ranked> {
  if (cost_span_ == CostSpan::window) {
    return ledger_.window(options_.ranked);
  }
  const std::optional<std::size_t> index = selected_frame();
  if (!index.has_value()) {
    return {};
  }
  return history_.at(*index).top;
}

auto InspectorModel::breakdown(std::size_t index) const -> std::string {
  return runtime::FrameBreakdown::describe(history_.at(index).timings,
                                           std::nullopt);
}

auto InspectorModel::counters(std::size_t index) const -> std::vector<Counter> {
  const runtime::FrameTimings& frame = history_.at(index).timings;
  return {
      Counter{"Frame", std::format("{}", history_.at(index).number)},
      Counter{"Quads", std::format("{}", frame.quads)},
      Counter{"Commands", std::format("{}", frame.commands)},
      Counter{"Recorded", std::format("{}", frame.recorded)},
      Counter{"Spliced", std::format("{}", frame.spliced)},
      Counter{"Culled", std::format("{}", frame.culled)},
      Counter{"Damaged pixels", std::format("{}", frame.damaged_pixels)},
      Counter{"Uploaded bytes", std::format("{}", frame.uploaded_bytes)},
      Counter{"Backdrops", std::format("{}", frame.backdrops)},
      Counter{"Blurs", std::format("{}", frame.blurs)},
      Counter{"Layer renders", std::format("{}", frame.layer_renders)},
      Counter{"Missed vsyncs", std::format("{}", frame.missed_vsyncs)},
  };
}

auto InspectorModel::bounds(composition::NodeId node) const
    -> std::optional<composition::Rect> {
  return introspection().node(node).transform(
      [](const runtime::Introspection::NodeRecord& record) {
        return record.bounds;
      });
}

auto InspectorModel::accessibility(composition::NodeId node) const
    -> std::optional<components::AccessibilityElement> {
  return introspection().accessibility(node);
}

auto InspectorModel::appearance() const
    -> std::optional<appearance::AppearanceModel> {
  return application_.get().appearance();
}

void InspectorModel::show_tokens(appearance::TokenGroup group) {
  if (token_group_ == group) {
    return;
  }
  token_group_ = group;
  token_.reset();
  bump(state_);
}

void InspectorModel::toggle_tokens(appearance::TokenGroup group) {
  if (token_group_ == group) {
    token_group_.reset();
    token_.reset();
    bump(state_);
    return;
  }
  show_tokens(group);
}

void InspectorModel::select_token(std::optional<std::size_t> token) {
  if (token_ == token) {
    return;
  }
  token_ = token;
  bump(state_);
}

void InspectorModel::toggle_token(std::size_t token) {
  select_token(token_ == token ? std::nullopt : std::optional{token});
}

auto InspectorModel::token_scheme() const -> theme::ColorScheme {
  const std::optional<appearance::AppearanceModel> model = appearance();
  if (!model.has_value()) {
    return theme::ColorScheme::light;
  }
  return model->settings().get().resolved_scheme().value_or(
      theme::ColorScheme::light);
}

auto InspectorModel::edit_token(std::size_t token,
                                const appearance::TokenValue& value) -> bool {
  const std::optional<appearance::AppearanceModel> model = appearance();
  if (!model.has_value() || token >= appearance::ThemeToken::all().size()) {
    return false;
  }
  return model->edit(appearance::ThemeToken::at(token), value);
}

auto InspectorModel::undo_token(std::size_t token) -> bool {
  const std::optional<appearance::AppearanceModel> model = appearance();
  if (!model.has_value() || token >= appearance::ThemeToken::all().size()) {
    return false;
  }
  return model->undo(appearance::ThemeToken::at(token));
}

void InspectorModel::reset_tokens() {
  if (const std::optional<appearance::AppearanceModel> model = appearance()) {
    model->reset_edits();
  }
}

auto InspectorModel::export_tokens()
    -> std::expected<appearance::AppearanceModel::Export,
                     appearance::ThemeError> {
  const std::optional<appearance::AppearanceModel> model = appearance();
  if (!model.has_value()) {
    return std::unexpected(
        appearance::ThemeError{appearance::ThemeErrorCode::not_found,
                               {},
                               0U,
                               {},
                               "the application has no appearance model"});
  }
  auto exported = options_.exports.has_value()
                      ? model->export_edits(options_.exports->get())
                      : model->export_edits();
  export_status_ = exported.has_value() ? exported->location
                                        : exported.error().description();
  bump(state_);
  return exported;
}

}
