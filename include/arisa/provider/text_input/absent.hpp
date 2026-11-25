#pragma once

#include <arisa/provider/text_input/provider.hpp>

namespace arisa::provider {

class AbsentTextInputProvider final : public TextInputProvider {
 public:
  [[nodiscard]] auto begin(TextInputRequest request)
      -> std::expected<TextInputSession, ProviderError> override;

  auto unhonoured(const TextInputRequest& request) const
      -> TextInputTraitSet override;
};

}
