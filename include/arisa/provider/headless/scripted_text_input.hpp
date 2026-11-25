#pragma once

#include <arisa/provider/text_input/provider.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace arisa::provider::headless {

class ScriptedTextInput final : public TextInputProvider {
 public:
  ScriptedTextInput();

  [[nodiscard]] auto begin(TextInputRequest request)
      -> std::expected<TextInputSession, ProviderError> override;

  auto unhonoured(const TextInputRequest& request) const
      -> TextInputTraitSet override;

  void push(TextInputOutcome outcome, std::uint32_t polls_before = 0);

  void push(TextInputOutcome outcome, std::vector<std::string> drafts);

  void refuse(TextInputTraitSet traits);

  auto requests() const -> std::vector<TextInputRequest>;

  auto live_sessions() const -> std::size_t;

  auto cancelled_sessions() const -> std::size_t;

 private:
  class Ledger;
  class Session;

  std::shared_ptr<Ledger> ledger_;
};

}
