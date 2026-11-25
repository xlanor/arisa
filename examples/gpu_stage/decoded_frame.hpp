#pragma once

#include <arisa/support/scoped_callback.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>

namespace arisa::examples {

class DecodedFrame {
 public:
  struct Picture {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::chrono::nanoseconds timestamp{0};
  };

  struct Planes {
    std::span<std::byte> luma;
    std::size_t luma_pitch{0};
    std::span<std::byte> chroma;
    std::size_t chroma_pitch{0};
  };

  struct Resident {
    const std::byte* backing{nullptr};
    std::size_t backing_size{0};
    std::size_t luma_offset{0};
    std::size_t luma_pitch{0};
    std::size_t chroma_offset{0};
    std::size_t chroma_pitch{0};
  };

  using Copy =
      std::move_only_function<std::expected<void, std::string>(const Planes&)>;

  DecodedFrame(Picture picture, Copy copy, support::ScopedCallback release,
               std::optional<Resident> resident = std::nullopt) noexcept;

  auto picture() const noexcept -> const Picture& { return picture_; }

  auto resident() const noexcept -> const std::optional<Resident>& {
    return resident_;
  }

  [[nodiscard]] auto copy_to(const Planes& planes)
      -> std::expected<void, std::string>;

 private:
  static auto fits(std::span<const std::byte> plane, std::size_t pitch,
                   std::size_t row, std::size_t rows) noexcept -> bool;

  Picture picture_;
  Copy copy_;
  support::ScopedCallback release_;
  std::optional<Resident> resident_;
};

}
