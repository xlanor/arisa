#pragma once

#include <arisa/composition/command.hpp>
#include <arisa/composition/command_hint.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/event_modifiers.hpp>
#include <arisa/composition/hover_path.hpp>
#include <arisa/composition/input_modality.hpp>
#include <arisa/composition/key_press.hpp>
#include <arisa/composition/keyboard_shortcut.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/pointer_style.hpp>
#include <arisa/composition/sensory_feedback.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/interaction/interaction_options.hpp>
#include <arisa/provider/audio/provider.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/navigation_tuning.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/input/stick.hpp>
#include <arisa/provider/surface/surface_scale.hpp>

#include <interaction/drag_session.hpp>
#include <interaction/focus_engine.hpp>
#include <interaction/gesture_arena.hpp>
#include <interaction/invocation.hpp>
#include <interaction/press_tracker.hpp>
#include <interaction/scroll_coordinator.hpp>
#include <interaction/stick_navigation.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::interaction {

class InputRouter {
 public:
  struct Shake {
    composition::NodeId node{0};
    composition::MoveCommandDirection direction{
        composition::MoveCommandDirection::right};
    std::chrono::nanoseconds started{0};
  };

  struct Pointer {
    composition::Point location{};
    composition::PointerStyle style{};
    bool visible{false};

    friend auto operator==(const Pointer&, const Pointer&) -> bool = default;
  };

  struct Outcome {
    bool changed{false};
    std::vector<Invocation> invocations;
    std::vector<provider::SoundRole> sounds;
  };

  static constexpr std::chrono::milliseconds shake_duration{320};
  static constexpr float trackpad_spread = 50.0F;
  static constexpr std::chrono::milliseconds type_select_timeout{1000};

  explicit InputRouter(InteractionOptions options = {}) noexcept
      : options_(options), engine_(SpatialSearch{options.alignment_bias}) {}

  auto update(const provider::InputFrame& input, composition::Tree& tree,
              provider::SurfaceScale scale, std::chrono::nanoseconds now,
              const provider::NavigationTuning& tuning) -> Outcome;

  void interrupt(composition::Tree& tree);

  auto take_keyboard_dismissal() noexcept -> bool {
    return scrolls_.take_keyboard_dismissal();
  }

  auto focused() const noexcept -> std::optional<composition::NodeId> {
    return engine_.focused();
  }

  auto focus(composition::Tree& tree, composition::NodeId node) -> bool {
    return engine_.focus(tree, node);
  }

  auto pressed() const noexcept -> std::optional<composition::NodeId> {
    return pressed_.has_value() ? pressed_ : arena_.pressed();
  }

  auto touched() const -> std::optional<composition::NodeId> {
    return arena_.touched();
  }

  auto focus_visible() const noexcept -> bool {
    if (pointing_) {
      return hover_target_.has_value() && hover_target_ == engine_.focused();
    }
    return !options_.hides_focus_on_touch ||
           modality_ != composition::InputModality::touch;
  }

  auto pointing() const noexcept -> bool { return pointing_; }

  auto pointer() const -> std::optional<Pointer>;

  auto hover_path() const noexcept
      -> const std::shared_ptr<const composition::HoverPath>& {
    return hover_path_;
  }

  auto modality() const noexcept -> composition::InputModality {
    return modality_;
  }

  auto hints() const noexcept -> const std::vector<composition::CommandHint>& {
    return hints_;
  }

  auto tilt() const noexcept -> provider::Stick { return tilt_; }

  auto contact() const noexcept -> std::optional<provider::PixelPoint> {
    return contact_;
  }

  auto shake() const noexcept -> const std::optional<Shake>& { return shake_; }

  auto options() const noexcept -> const InteractionOptions& {
    return options_;
  }

  auto engine() const noexcept -> const FocusEngine& { return engine_; }

  auto scrolls() const noexcept -> const ScrollCoordinator& { return scrolls_; }

  void set_horizontally_compact(bool compact) noexcept {
    scrolls_.set_horizontally_compact(compact);
  }

  auto arena() const noexcept -> const GestureArena& { return arena_; }

  auto drag() const -> std::optional<DragSession::View> { return drag_.view(); }

  auto hovered() const noexcept -> std::optional<composition::NodeId> {
    return hovered_;
  }

 private:
  struct Gamepad {
    provider::Buttons buttons{};
    provider::Buttons physical{};
    provider::Stick left{};
    provider::Stick right{};
  };

  struct Trackpad {
    composition::Point centre{};
    float spread{0.0F};
  };

  enum class Carry : std::uint8_t {
    pick,
    drop,
    cancel,
  };

  struct Binding {
    std::shared_ptr<composition::Command> command;
    std::optional<composition::NodeId> activation;
    std::shared_ptr<composition::Command> held;
    std::optional<Carry> carry{};
    std::optional<composition::NodeId> source{};
  };

  static auto strongest(provider::Stick current,
                        provider::Stick candidate) noexcept -> provider::Stick;

  auto merge(const provider::InputFrame& input) -> Gamepad;

  void sense(const provider::InputFrame& input, const Gamepad& pad,
             const provider::NavigationTuning& tuning);

  auto heading(const Gamepad& pad, const provider::NavigationTuning& tuning)
      -> std::optional<composition::MoveCommandDirection>;

  void navigate(composition::Tree& tree,
                composition::MoveCommandDirection direction, bool initial,
                std::chrono::nanoseconds now, Outcome& outcome);

  void steer(composition::Tree& tree, const Gamepad& pad,
             std::chrono::nanoseconds now,
             const provider::NavigationTuning& tuning, Outcome& outcome);

  void buttons(composition::Tree& tree, const Gamepad& pad,
               std::chrono::nanoseconds now,
               const provider::NavigationTuning& tuning, Outcome& outcome);

  void fire(const Binding& binding, composition::CommandTrigger trigger,
            Outcome& outcome);

  void touches(composition::Tree& tree, const provider::InputFrame& input,
               provider::SurfaceScale scale, std::chrono::nanoseconds now,
               Outcome& outcome);

  void carry(composition::Tree& tree, GestureArena::Effects& effects,
             Outcome& outcome);

  void pinch(composition::Tree& tree, const provider::TrackpadPinch& pinch,
             composition::Point at, std::chrono::nanoseconds now,
             GestureArena::Effects& effects);

  void hover(composition::Tree& tree, const provider::InputFrame& input,
             provider::SurfaceScale scale, Outcome& outcome);

  void notify(const composition::Tree& tree,
              const std::vector<composition::NodeId>& chain, bool moved,
              Outcome& outcome);

  void restyle(const composition::Tree& tree,
               const std::vector<composition::NodeId>& chain);

  void follow(composition::Tree& tree,
              const std::vector<composition::NodeId>& chain,
              composition::Point point, Outcome& outcome);

  auto trace(const composition::Tree& tree,
             std::vector<composition::NodeId> chain) const
      -> composition::HoverPath;

  static auto hover_enabled(const composition::Tree& tree,
                            composition::NodeId node) -> bool;

  void type(composition::Tree& tree, const provider::InputFrame& input,
            std::chrono::nanoseconds now, Outcome& outcome);

  auto type_select(composition::Tree& tree, const composition::KeyPress& press,
                   std::chrono::nanoseconds now) -> bool;

  void track_modifiers(composition::Tree& tree, Outcome& outcome);

  static void collect_modifier_handlers(
      const composition::Node& node,
      std::vector<std::pair<composition::NodeId,
                            std::shared_ptr<composition::ModifierKeysHandler>>>&
          found);

  auto modifiers() const -> composition::EventModifiers;

  auto press_of(const provider::KeyEvent& event) const -> composition::KeyPress;

  auto deliver(composition::Tree& tree, const composition::KeyPress& press)
      -> bool;

  auto shortcut(const composition::Tree& tree,
                const composition::KeyPress& press) const
      -> std::optional<composition::NodeId>;

  auto equivalent(const composition::Tree& tree,
                  const provider::KeyEvent& event,
                  const composition::KeyPress& press, Outcome& outcome) const
      -> bool;

  static auto fires(const composition::KeyboardShortcut& shortcut,
                    const composition::KeyPress& press) -> bool;

  static auto seek(const composition::Tree& tree, const composition::Node& node,
                   const composition::KeyPress& press)
      -> std::optional<composition::NodeId>;

  void perform(composition::Tree& tree, const Binding& binding,
               Outcome& outcome);

  void scroll(composition::Tree& tree, const Gamepad& pad,
              std::optional<composition::NodeId> before,
              std::chrono::nanoseconds now,
              const provider::NavigationTuning& tuning, Outcome& outcome);

  auto turn(composition::Tree& tree, provider::Button button,
            std::chrono::nanoseconds now) -> bool;

  auto accompany(composition::Tree& tree) -> bool;

  void feel(composition::Tree& tree, composition::SensoryFeedback feedback);

  void blocked(composition::Tree& tree,
               composition::MoveCommandDirection direction,
               std::chrono::nanoseconds now, Outcome& outcome);

  struct Described {
    provider::Button button{provider::Button::a};
    std::string_view label;
    std::size_t rank{0};
    std::size_t order{0};
  };

  auto chain(const composition::Tree& tree)
      -> std::span<const composition::NodeId>;

  auto resolve(const composition::Tree& tree, provider::Button button)
      -> std::optional<Binding>;

  static auto holds(const composition::Command& command,
                    provider::Button button) noexcept -> bool;

  static auto trigger_of(const composition::CommandOptions& options) noexcept
      -> composition::CommandTrigger;

  static auto tuned(const std::optional<Binding>& binding,
                    const provider::NavigationTuning& tuning) noexcept
      -> provider::NavigationTuning;

  auto button_for(const composition::Command& command) const
      -> std::optional<provider::Button>;

  auto rank(provider::Button button) const noexcept -> std::size_t;

  auto describe(const composition::Tree& tree) -> bool;

  InteractionOptions options_;
  FocusEngine engine_;
  GestureArena arena_;
  ScrollCoordinator scrolls_;
  bool touch_focus_{false};
  StickNavigation stick_;
  PressTracker navigation_;
  std::optional<composition::MoveCommandDirection> heading_;
  std::array<PressTracker, provider::all_buttons.size()> trackers_{};
  std::array<std::optional<Binding>, provider::all_buttons.size()> bindings_{};
  std::set<provider::Key> keys_;
  std::set<provider::Key> consumed_;
  DragSession drag_;
  std::optional<provider::TouchId> second_touch_;
  std::optional<composition::Point> pinch_centre_;
  std::optional<Trackpad> trackpad_;
  std::optional<provider::PixelPoint> hover_seen_;
  std::optional<composition::NodeId> hovered_;
  bool pointing_{false};
  bool hover_pointing_{false};
  std::uint64_t hover_revision_{0};
  std::uint64_t hover_geometry_{0};
  std::uint64_t hover_arrangement_{0};
  composition::Point hover_point_{};
  std::vector<composition::NodeId> hover_chain_;
  std::vector<composition::NodeId> hover_inside_;
  std::vector<composition::NodeId> continuous_inside_;
  std::optional<composition::NodeId> hover_target_;
  composition::PointerStyle pointer_style_{};
  composition::Visibility pointer_visibility_{
      composition::Visibility::automatic};
  std::shared_ptr<const composition::HoverPath> hover_path_;
  provider::Buttons previous_buttons_{};
  std::optional<provider::PointerState> pointer_;
  std::optional<provider::TouchId> touch_;
  std::optional<provider::PixelPoint> contact_;
  bool pointer_down_{false};
  std::optional<composition::NodeId> pressed_;
  composition::InputModality modality_{composition::InputModality::controller};
  composition::InputModality arriving_{composition::InputModality::controller};
  composition::MoveCommandDirection last_move_{
      composition::MoveCommandDirection::right};
  std::vector<composition::CommandHint> hints_;
  std::vector<Described> described_;
  std::vector<composition::NodeId> chain_;
  provider::Stick tilt_{};
  std::optional<Shake> shake_;
  std::string type_select_;
  std::chrono::nanoseconds type_select_at_{0};
  composition::EventModifiers held_modifiers_{};
  std::optional<std::uint64_t> modifier_revision_;
  std::vector<composition::NodeId> modifier_nodes_;
  std::vector<std::pair<composition::NodeId,
                        std::shared_ptr<composition::ModifierKeysHandler>>>
      modifier_handlers_;
};

}
