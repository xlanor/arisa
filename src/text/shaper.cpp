#include <text/shaper.hpp>

#include <hb.h>

#include <utility>

namespace arisa::text {

void Shaper::BufferRelease::release(handle_type& handle) noexcept {
  hb_buffer_destroy(handle);
}

Shaper::Shaper(support::UniqueHandle<BufferRelease> buffer) noexcept
    : buffer_(std::move(buffer)) {}

auto Shaper::create() -> std::expected<Shaper, TextError> {
  support::UniqueHandle<BufferRelease> buffer{hb_buffer_create()};
  if (hb_buffer_allocation_successful(buffer.get()) == 0) {
    return std::unexpected(TextError{TextErrorCode::font_engine});
  }
  return Shaper{std::move(buffer)};
}

auto Shaper::shape(const FontFace& face, const ShapeRequest& request)
    -> std::vector<ShapedGlyph> {
  hb_buffer_t* buffer = buffer_.get();
  hb_buffer_clear_contents(buffer);
  hb_buffer_add_utf8(buffer, request.text.data(),
                     static_cast<int>(request.text.size()),
                     static_cast<unsigned int>(request.start),
                     static_cast<int>(request.end - request.start));
  hb_buffer_set_direction(
      buffer, request.right_to_left ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
  hb_buffer_set_script(buffer, hb_script_from_iso15924_tag(request.script));
  if (!request.language.empty()) {
    hb_buffer_set_language(
        buffer,
        hb_language_from_string(request.language.data(),
                                static_cast<int>(request.language.size())));
  }
  unsigned int flags = HB_BUFFER_FLAG_DEFAULT;
  if (request.start == 0) {
    flags |= HB_BUFFER_FLAG_BOT;
  }
  if (request.end == request.text.size()) {
    flags |= HB_BUFFER_FLAG_EOT;
  }
  hb_buffer_set_flags(buffer, static_cast<hb_buffer_flags_t>(flags));
  hb_buffer_set_cluster_level(buffer,
                              HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
  const hb_feature_t tabular{.tag = HB_TAG('t', 'n', 'u', 'm'),
                             .value = 1,
                             .start = HB_FEATURE_GLOBAL_START,
                             .end = HB_FEATURE_GLOBAL_END};
  hb_shape(face.shaping_font(), buffer,
           request.tabular_digits ? &tabular : nullptr,
           request.tabular_digits ? 1U : 0U);
  unsigned int count = 0;
  const hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &count);
  const hb_glyph_position_t* positions =
      hb_buffer_get_glyph_positions(buffer, &count);
  const float scale =
      request.size.value() / static_cast<float>(face.units_per_em());
  const float across = scale * face.options().stretch;
  std::vector<ShapedGlyph> glyphs;
  glyphs.reserve(count);
  for (unsigned int index = 0; index < count; ++index) {
    const hb_glyph_info_t& info = infos[index];
    const hb_glyph_position_t& position = positions[index];
    glyphs.push_back(ShapedGlyph{
        .glyph = info.codepoint,
        .cluster = info.cluster,
        .advance = static_cast<float>(position.x_advance) * across,
        .x_offset = static_cast<float>(position.x_offset) * across,
        .y_offset = static_cast<float>(position.y_offset) * scale,
        .unsafe_to_break =
            (static_cast<unsigned int>(hb_glyph_info_get_glyph_flags(&info)) &
             static_cast<unsigned int>(HB_GLYPH_FLAG_UNSAFE_TO_BREAK)) != 0});
  }
  return glyphs;
}

}
