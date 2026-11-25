#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/text_input/text_input_session.hpp>
#include <arisa/provider/text_input/text_input_traits.hpp>
#include <arisa/support/bounded_value.hpp>

#include <cstdint>
#include <expected>
#include <string>

namespace arisa::provider {

struct TextInputLimitTag;

using TextInputLimit =
    support::BoundedValue<std::uint32_t, 1, 4096, TextInputLimitTag>;

enum class TextInputKind : std::uint8_t {
  text,
  password,
};

struct TextInputRequest {
  std::string initial;
  std::string hint;
  TextInputLimit max_length{TextInputLimit::of<256>()};
  TextInputKind kind{TextInputKind::text};
  TextInputTraits traits{};
  bool live{false};

  friend auto operator==(const TextInputRequest&, const TextInputRequest&)
      -> bool = default;
};

class TextInputProvider {
 public:
  virtual ~TextInputProvider() = default;

  [[nodiscard]] virtual auto begin(TextInputRequest request)
      -> std::expected<TextInputSession, ProviderError> = 0;

  virtual auto unhonoured(const TextInputRequest& request) const
      -> TextInputTraitSet = 0;
};

}
