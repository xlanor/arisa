#pragma once

#include <arisa/components/image.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/image.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

namespace arisa::components {

class AsyncImagePhase {
 public:
  enum class Kind : std::uint8_t { empty, success, failure };

  AsyncImagePhase() noexcept = default;

  static auto success(std::shared_ptr<const provider::Image> pixels,
                      float scale = 1.0F) -> AsyncImagePhase;

  static auto failure(provider::ProviderError error) -> AsyncImagePhase;

  auto kind() const noexcept -> Kind;

  auto empty() const noexcept -> bool { return kind() == Kind::empty; }

  auto image() const -> std::optional<Image>;

  auto pixels() const noexcept -> std::shared_ptr<const provider::Image>;

  auto error() const noexcept -> std::optional<provider::ProviderError>;

  friend auto operator==(const AsyncImagePhase&, const AsyncImagePhase&)
      -> bool = default;

 private:
  struct Loaded {
    std::shared_ptr<const provider::Image> pixels;
    float scale{1.0F};

    friend auto operator==(const Loaded&, const Loaded&) -> bool = default;
  };

  std::variant<std::monostate, Loaded, provider::ProviderError> state_;
};

}
