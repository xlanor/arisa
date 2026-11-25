#pragma once

#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/theme_error.hpp>
#include <arisa/appearance/theme_token.hpp>
#include <arisa/components/accessibility_tree.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/inspector/cost_ledger.hpp>
#include <arisa/inspector/frame_history.hpp>
#include <arisa/inspector/inspector_options.hpp>
#include <arisa/inspector/inspector_panel.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/frame_observer.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/runtime/introspection.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <flat_set>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::inspector {

class InspectorModel final
    : public runtime::FrameObserver,
      public std::enable_shared_from_this<InspectorModel> {
 public:
  enum class CostSpan : std::uint8_t {
    window,
    frame,
  };

  struct Row {
    runtime::Introspection::NodeRecord record;
    bool expandable{false};
    bool expanded{false};
  };

  struct Indentation {
    std::size_t base{0};
    float step{0.0F};
    float content{0.0F};

    auto indent(std::size_t depth) const noexcept -> float {
      return step * static_cast<float>(depth > base ? depth - base : 0);
    }
  };

  using Counter = std::pair<std::string_view, std::string>;

  static constexpr float cursor_step = 6.0F;
  static constexpr float panel_share = 0.5F;
  static constexpr float indent_step = 12.0F;
  static constexpr float label_share = 0.6F;
  static constexpr float row_inset = 48.0F;
  static constexpr float disclosure_width = 36.0F;
  static constexpr float row_spacing = 4.0F;

  InspectorModel(runtime::Application& application, InspectorOptions options);

  void filter(provider::InputFrame& input) override;

  void observe(const runtime::FrameTimings& frame) override;

  void open();

  void close();

  void toggle();

  auto is_open() const noexcept -> bool { return open_; }

  void show(InspectorPanel panel);

  auto panel() const noexcept -> InspectorPanel { return panel_; }

  void select(std::optional<composition::NodeId> node);

  auto selected() const noexcept -> std::optional<composition::NodeId> {
    return selected_;
  }

  void toggle_expanded(composition::NodeId node);

  auto expanded(composition::NodeId node) const -> bool;

  void set_paused(bool paused);

  auto paused() const noexcept -> bool { return paused_; }

  auto pick(composition::Point point) -> std::optional<composition::NodeId>;

  void move_cursor(composition::Point to);

  auto cursor() const noexcept -> composition::Point { return cursor_; }

  auto hovered() const noexcept -> std::optional<composition::NodeId> {
    return hovered_;
  }

  void select_frame(std::optional<std::uint64_t> number);

  void step_frame(int offset);

  auto selected_frame() const -> std::optional<std::size_t>;

  void set_cost_span(CostSpan span);

  auto cost_span() const noexcept -> CostSpan { return cost_span_; }

  auto history() const noexcept -> const FrameHistory& { return history_; }

  auto own_frames() const noexcept -> std::uint64_t { return own_frames_; }

  auto options() const noexcept -> const InspectorOptions& { return options_; }

  auto panel_width() const -> float;

  auto introspection() const -> const runtime::Introspection&;

  auto focus_snapshot() const noexcept
      -> const runtime::Introspection::FocusRecord& {
    return focus_;
  }

  auto rows() const -> std::vector<Row>;

  auto indentation(const std::vector<Row>& rows) const -> Indentation;

  auto describe(composition::NodeId node) const -> std::string;

  auto ranked() const -> std::vector<CostLedger::Ranked>;

  auto breakdown(std::size_t index) const -> std::string;

  auto counters(std::size_t index) const -> std::vector<Counter>;

  auto bounds(composition::NodeId node) const
      -> std::optional<composition::Rect>;

  auto accessibility(composition::NodeId node) const
      -> std::optional<components::AccessibilityElement>;

  auto appearance() const -> std::optional<appearance::AppearanceModel>;

  void show_tokens(appearance::TokenGroup group);

  void toggle_tokens(appearance::TokenGroup group);

  auto token_group() const noexcept -> std::optional<appearance::TokenGroup> {
    return token_group_;
  }

  void select_token(std::optional<std::size_t> token);

  void toggle_token(std::size_t token);

  auto selected_token() const noexcept -> std::optional<std::size_t> {
    return token_;
  }

  auto token_scheme() const -> theme::ColorScheme;

  auto edit_token(std::size_t token, const appearance::TokenValue& value)
      -> bool;

  auto undo_token(std::size_t token) -> bool;

  void reset_tokens();

  [[nodiscard]] auto export_tokens()
      -> std::expected<appearance::AppearanceModel::Export,
                       appearance::ThemeError>;

  auto export_status() const noexcept -> const std::string& {
    return export_status_;
  }

  auto state() const noexcept -> const composition::Observable<std::uint64_t>& {
    return state_;
  }

  auto frames() const noexcept
      -> const composition::Observable<std::uint64_t>& {
    return frames_;
  }

  auto structure() const noexcept
      -> const composition::Observable<std::uint64_t>& {
    return structure_;
  }

  static auto label(const runtime::Introspection::NodeRecord& record)
      -> std::string;

 private:
  static auto held(const provider::InputFrame& input,
                   provider::Buttons buttons) noexcept -> bool;

  static auto pressed(const provider::InputFrame& input) noexcept
      -> provider::Buttons;

  static void strip(provider::InputFrame& input, provider::Buttons buttons);

  auto toggled(provider::InputFrame& input) -> bool;

  void steer(provider::InputFrame& input, provider::Buttons previous);

  auto inside_panel(composition::Point point) const -> bool;

  void reveal(composition::NodeId node);

  void bump(composition::Observable<std::uint64_t>& observable);

  std::reference_wrapper<runtime::Application> application_;
  InspectorOptions options_;
  bool open_{false};
  InspectorPanel panel_{InspectorPanel::tree};
  std::optional<composition::NodeId> selected_;
  std::optional<composition::NodeId> hovered_;
  composition::Point cursor_{};
  bool paused_{false};
  CostSpan cost_span_{CostSpan::window};
  std::optional<appearance::TokenGroup> token_group_{
      appearance::TokenGroup::colours};
  std::optional<std::size_t> token_;
  std::string export_status_;
  std::optional<std::uint64_t> selected_frame_;
  std::flat_set<composition::NodeId> expanded_;
  std::flat_set<composition::NodeId> collapsed_;
  runtime::Introspection::FocusRecord focus_;
  FrameHistory history_;
  CostLedger ledger_;
  std::uint64_t own_frames_{0};
  provider::Buttons previous_{};
  bool chord_held_{false};
  composition::Observable<std::uint64_t> state_{0};
  composition::Observable<std::uint64_t> frames_{0};
  composition::Observable<std::uint64_t> structure_{0};
};

}
