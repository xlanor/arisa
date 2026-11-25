#pragma once

#include <arisa/support/unique_handle.hpp>
#include <arisa/text/font_face.hpp>
#include <arisa/text/point_size.hpp>
#include <arisa/text/text_error.hpp>
#include <text/shaped_glyph.hpp>

#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>

struct hb_buffer_t;

namespace arisa::text {

struct ShapeRequest {
  std::string_view text;
  std::uint32_t start{0};
  std::uint32_t end{0};
  bool right_to_left{false};
  std::uint32_t script{0};
  std::string_view language;
  PointSize size{PointSize::of<17.0F>()};
  bool tabular_digits{false};
};

class Shaper {
 public:
  [[nodiscard]] static auto create() -> std::expected<Shaper, TextError>;

  auto shape(const FontFace& face, const ShapeRequest& request)
      -> std::vector<ShapedGlyph>;

 private:
  struct BufferRelease {
    using handle_type = hb_buffer_t*;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type handle) noexcept -> bool {
      return handle != nullptr;
    }
    static void release(handle_type& handle) noexcept;
  };

  explicit Shaper(support::UniqueHandle<BufferRelease> buffer) noexcept;

  support::UniqueHandle<BufferRelease> buffer_;
};

}
