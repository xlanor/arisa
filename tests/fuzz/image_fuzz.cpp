#include <arisa/provider/gpu/image.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                       std::size_t size) -> int {
  const auto decoded = arisa::provider::Image::decode(
      std::as_bytes(std::span<const std::uint8_t>{data, size}));
  if (decoded.has_value()) {
    static_cast<void>(
        decoded->pixel(decoded->width() - 1, decoded->height() - 1));
  }
  return 0;
}
