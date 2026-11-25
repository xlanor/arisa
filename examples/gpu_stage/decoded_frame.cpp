#include <decoded_frame.hpp>

#include <utility>

namespace arisa::examples {

DecodedFrame::DecodedFrame(Picture picture, Copy copy,
                           support::ScopedCallback release,
                           std::optional<Resident> resident) noexcept
    : picture_(picture),
      copy_(std::move(copy)),
      release_(std::move(release)),
      resident_(resident) {}

auto DecodedFrame::fits(std::span<const std::byte> plane, std::size_t pitch,
                        std::size_t row, std::size_t rows) noexcept -> bool {
  if (rows == 0 || row == 0) {
    return true;
  }
  return pitch >= row && plane.size() >= row &&
         (rows - 1) <= (plane.size() - row) / pitch;
}

auto DecodedFrame::copy_to(const Planes& planes)
    -> std::expected<void, std::string> {
  const std::size_t width = picture_.width;
  const std::size_t height = picture_.height;
  const std::size_t chroma_row = ((width + 1) / 2) * 2;
  const std::size_t chroma_rows = (height + 1) / 2;
  if (!fits(planes.luma, planes.luma_pitch, width, height) ||
      !fits(planes.chroma, planes.chroma_pitch, chroma_row, chroma_rows)) {
    return std::unexpected(
        std::string{"the destination planes are smaller than the frame"});
  }
  if (!copy_) {
    return std::unexpected(std::string{"the frame has no pixels"});
  }
  return copy_(planes);
}

}
