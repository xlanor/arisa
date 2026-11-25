#include <arisa/composition/context.hpp>
#include <arisa/composition/redaction.hpp>

#include <ranges>

namespace arisa::composition {

const EnvironmentKey<Redaction::Entry> Redaction::entry{"redaction", Entry{}};
const EnvironmentKey<bool> Redaction::privacy_sensitive{"privacy_sensitive",
                                                        false};

auto Redaction::reasons(Context& context) -> RedactionReasons {
  const auto chain = context.environment_chain(entry);
  return merged(chain);
}

auto Redaction::applied(RedactionReasons inherited, const Entry& scope) noexcept
    -> RedactionReasons {
  return scope.resets ? scope.reasons : inherited | scope.reasons;
}

auto Redaction::merged(
    std::span<const std::reference_wrapper<const Entry>> chain)
    -> RedactionReasons {
  RedactionReasons reasons;
  for (const Entry& found : chain | std::views::reverse) {
    reasons = applied(reasons, found);
  }
  return reasons;
}

}
