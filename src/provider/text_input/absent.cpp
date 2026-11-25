#include <arisa/provider/text_input/absent.hpp>

namespace arisa::provider {

auto AbsentTextInputProvider::begin(TextInputRequest)
    -> std::expected<TextInputSession, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

auto AbsentTextInputProvider::unhonoured(const TextInputRequest& request) const
    -> TextInputTraitSet {
  return request.traits.requested();
}

}
