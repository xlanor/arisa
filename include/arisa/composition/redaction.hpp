#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/redaction_reasons.hpp>

#include <functional>
#include <span>

namespace arisa::composition {

class Context;

class Redaction {
 public:
  struct Entry {
    RedactionReasons reasons{};
    bool resets{false};

    friend constexpr auto operator==(const Entry&, const Entry&)
        -> bool = default;
  };

  static const EnvironmentKey<Entry> entry;
  static const EnvironmentKey<bool> privacy_sensitive;

  static auto reasons(Context& context) -> RedactionReasons;

  static auto applied(RedactionReasons inherited, const Entry& scope) noexcept
      -> RedactionReasons;

  static auto merged(std::span<const std::reference_wrapper<const Entry>> chain)
      -> RedactionReasons;
};

}
