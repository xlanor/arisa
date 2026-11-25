#pragma once

#include <arisa/provider/text_input/provider.hpp>
#include <arisa/provider/threading/provider.hpp>

#include <atomic>
#include <expected>
#include <memory>

namespace arisa::provider::horizon {

class HorizonTextInput final : public TextInputProvider {
 public:
  [[nodiscard]] static auto create(ThreadingProvider& threading)
      -> std::expected<HorizonTextInput, ProviderError>;

  [[nodiscard]] static auto create(ThreadingProvider& threading,
                                   std::shared_ptr<std::atomic<bool>> shown)
      -> std::expected<HorizonTextInput, ProviderError>;

  HorizonTextInput(const HorizonTextInput&) = delete;
  auto operator=(const HorizonTextInput&) -> HorizonTextInput& = delete;
  HorizonTextInput(HorizonTextInput&& other) noexcept;
  auto operator=(HorizonTextInput&&) -> HorizonTextInput& = delete;
  ~HorizonTextInput() override;

  [[nodiscard]] auto begin(TextInputRequest request)
      -> std::expected<TextInputSession, ProviderError> override;

  auto unhonoured(const TextInputRequest& request) const
      -> TextInputTraitSet override;

 private:
  class Exchange;
  class Keyboard;
  class Session;

  HorizonTextInput(std::unique_ptr<Keyboard> keyboard, Thread thread) noexcept;

  std::unique_ptr<Keyboard> keyboard_;
  Thread thread_;
};

}
