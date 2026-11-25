#include <arisa/provider/threading/thread_placement.hpp>

namespace arisa::provider {

auto ThreadPlacement::create(CoreMask cores, ThreadPriority priority,
                             StackSize stack)
    -> std::expected<ThreadPlacement, ProviderError> {
  const bool cores_valid = cores.value() != 0;
  const bool stack_valid =
      stack.value() >= minimum_stack && stack.value() % stack_alignment == 0;
  if (!cores_valid || !stack_valid) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  return ThreadPlacement{cores, priority, stack};
}

}
