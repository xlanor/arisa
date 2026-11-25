#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/runtime/text_input.hpp>

#include <string>
#include <string_view>

namespace arisa::examples {

class CounterScreen final : public composition::Component {
 public:
  static constexpr composition::StateKey<int> count{"count", 0};
  static constexpr composition::StateKey<int> uptime{"uptime", 0};
  static constexpr composition::StateKey<int> name_requests{"name_requests", 0};
  static constexpr composition::StateKey<bool> exploding{"exploding", false};
  static constexpr composition::StateKey<int> attempt{"attempt", 0};
  static constexpr composition::EnvironmentKey<std::string_view> accent{
      "accent", "plain"};
  static constexpr std::string_view increment = "increment";
  static constexpr std::string_view name_button = "name";
  static constexpr std::string_view fail_button = "fail";
  static constexpr std::string_view retry_button = "retry";
  static constexpr std::string_view accent_name = "arisa blue";

  class Badge final : public composition::Component {
   public:
    auto operator==(const Badge&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override;
  };

  class Label final : public composition::Component {
   public:
    auto operator==(const Label&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override;
  };

  class Fragile final : public composition::Component {
   public:
    explicit Fragile(bool failing) noexcept : exploding_(failing) {}

    auto operator==(const Fragile&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override;

   private:
    bool exploding_;
  };

  explicit CounterScreen(runtime::TextInput text_input) noexcept
      : text_input_(text_input) {}

  auto operator==(const CounterScreen&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static constexpr composition::StateKey<std::string> name{"name", "(none)"};

  static auto tick(composition::StateHandle<int> seconds) -> async::Task<void>;

  static auto ask(int request, runtime::TextInput text_input,
                  composition::StateHandle<std::string> shown)
      -> async::Task<void>;

  runtime::TextInput text_input_;
};

}
