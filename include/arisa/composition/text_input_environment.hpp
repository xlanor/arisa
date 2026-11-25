#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/find_context.hpp>
#include <arisa/composition/preference_key.hpp>
#include <arisa/composition/text_input_formatting_control_placement.hpp>
#include <arisa/composition/visibility.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace arisa::composition {

class TextInputEnvironment {
 public:
  using Builder = std::function<Element()>;
  using Completer = std::function<void(const std::string&)>;

  class Completion {
   public:
    void set(Completer filled) { completer_ = std::move(filled); }

    void operator()(const std::string& completion) const {
      if (completer_) {
        completer_(completion);
      }
    }

   private:
    Completer completer_;
  };

  struct FormattingControls {
    Visibility accessory_bar{Visibility::automatic};
    Visibility context_menu{Visibility::automatic};
    Visibility input_assistant{Visibility::automatic};

    auto with(Visibility visibility,
              TextInputFormattingControlPlacement::Set placements) const
        -> FormattingControls {
      FormattingControls changed = *this;
      using Set = TextInputFormattingControlPlacement::Set;
      if (placements.contains(Set::accessory_bar)) {
        changed.accessory_bar = visibility;
      }
      if (placements.contains(Set::context_menu)) {
        changed.context_menu = visibility;
      }
      if (placements.contains(Set::input_assistant)) {
        changed.input_assistant = visibility;
      }
      return changed;
    }

    friend auto operator==(const FormattingControls&, const FormattingControls&)
        -> bool = default;
  };

  static const EnvironmentKey<std::shared_ptr<Completion>> completer;
  static const EnvironmentKey<FormattingControls> formatting_controls;
  static const EnvironmentKey<std::optional<FindContext>> find_context;
  static const EnvironmentKey<bool> find_disabled;
  static const EnvironmentKey<bool> replace_disabled;
  static const PreferenceKey<bool> keyboard;
};

}
