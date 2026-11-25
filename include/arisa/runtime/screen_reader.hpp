#pragma once

#include <arisa/components/accessibility_tree.hpp>
#include <arisa/composition/accessibility_rotor.hpp>
#include <arisa/composition/accessibility_speech.hpp>
#include <arisa/composition/accessibility_technologies.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/speech/provider.hpp>
#include <arisa/provider/speech/utterance.hpp>
#include <arisa/theme/space.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::interaction {

class InputRouter;

}

namespace arisa::runtime {

class ScreenReader {
 public:
  static constexpr std::size_t history_limit = 32;
  static constexpr std::uint64_t caption_overlay =
      std::numeric_limits<std::uint64_t>::max() - 16U;
  static constexpr std::uint64_t large_content_overlay =
      std::numeric_limits<std::uint64_t>::max() - 17U;
  static constexpr provider::Button previous_item = provider::Button::zl;
  static constexpr provider::Button next_item = provider::Button::zr;
  static constexpr provider::Button next_rotor = provider::Button::left_stick;
  static constexpr std::string_view actions_rotor = "Actions";
  static constexpr std::string_view adjust_rotor = "Adjust value";
  static constexpr std::string_view activate_item = "Activate";
  static constexpr std::string_view rotor_label = "Rotor";
  static constexpr std::string_view previous_label = "Previous";
  static constexpr std::string_view next_label = "Next";
  static constexpr std::string_view caption_key = "arisa-screen-reader";

  ScreenReader(provider::SpeechProvider& speech, bool speaks) noexcept
      : speech_(speech),
        speaks_(speaks),
        pending_(std::make_shared<Commands>()) {}

  void configure(composition::Tree& tree,
                 composition::AccessibilityTechnologies technologies,
                 bool large_content);

  auto voice_over() const noexcept -> bool { return voice_over_; }

  auto switch_control() const noexcept -> bool { return switch_control_; }

  auto running() const noexcept -> bool {
    return voice_over_ || switch_control_;
  }

  auto active() const noexcept -> bool { return running() || large_content_; }

  void filter(provider::InputFrame& frame, provider::Button confirm,
              provider::Button back);

  auto follow(composition::Tree& tree, interaction::InputRouter& router)
      -> bool;

  void announce(std::string text, composition::AccessibilitySpeech speech = {});

  auto rotor() const noexcept -> std::string_view;

  auto rotors() const -> std::vector<std::string>;

  auto selection() const noexcept -> std::string_view { return selection_; }

  auto caption() const noexcept -> std::string_view { return caption_; }

  auto spoken() const noexcept -> std::span<const provider::Utterance> {
    return spoken_;
  }

  auto narrations() const noexcept -> std::uint64_t { return narrations_; }

  auto showing_large_content() const noexcept -> bool {
    return large_shown_.has_value();
  }

 private:
  enum class RotorKind : std::uint8_t {
    actions,
    adjust,
    custom,
    headings,
    links,
  };

  enum class ChoiceKind : std::uint8_t {
    activate,
    action,
    scroll,
    zoom,
    drag,
    drop,
  };

  struct Choice {
    ChoiceKind kind{ChoiceKind::activate};
    std::size_t index{0};
    theme::Edge edge{theme::Edge::top};
    bool inward{true};
    std::string name;
  };

  struct Rotor {
    RotorKind kind{RotorKind::actions};
    std::string name;
    std::optional<composition::AccessibilityRotor> custom{};
  };

  struct Commands {
    int step{0};
    bool next_rotor{false};
    bool perform{false};
    bool escape{false};
  };

  void rebuild(const components::AccessibilityTree& accessible);

  void step(composition::Tree& tree, interaction::InputRouter& router,
            const components::AccessibilityTree& accessible, int direction);

  void step_custom(composition::Tree& tree, interaction::InputRouter& router,
                   const components::AccessibilityTree& accessible,
                   const composition::AccessibilityRotor& rotor, int direction);

  void step_system(composition::Tree& tree, interaction::InputRouter& router,
                   const components::AccessibilityTree& accessible,
                   RotorKind kind, int direction);

  void perform(composition::Tree& tree);

  void escape(composition::Tree& tree);

  auto owns_confirm() const -> bool;

  auto has_escape() const -> bool;

  void show_caption(composition::Tree& tree);

  void show_large_content(composition::Tree& tree);

  static auto wrapped(std::size_t index, int direction, std::size_t count)
      -> std::size_t;

  std::reference_wrapper<provider::SpeechProvider> speech_;
  bool speaks_;
  bool voice_over_{false};
  bool switch_control_{false};
  bool large_content_{false};
  provider::Buttons held_{};
  provider::Buttons owned_{};
  bool inject_move_{false};
  std::shared_ptr<Commands> pending_;
  std::optional<composition::NodeId> read_;
  std::optional<components::AccessibilityElement> current_;
  std::vector<Rotor> rotors_;
  std::vector<Choice> choices_;
  std::size_t rotor_{0};
  std::size_t item_{0};
  std::size_t entry_{0};
  bool reread_{false};
  bool caption_dirty_{false};
  std::optional<composition::NodeId> large_shown_;
  std::vector<provider::Utterance> spoken_;
  std::string caption_;
  std::string selection_;
  std::uint64_t narrations_{0};
};

}
