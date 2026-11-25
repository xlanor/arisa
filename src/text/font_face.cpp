#include <arisa/text/font_face.hpp>

#include <text/shaper.hpp>

#include <freetype/freetype.h>
#include <freetype/ftmm.h>
#include <freetype/ftoutln.h>
#include <freetype/tttables.h>
#include <ft2build.h>
#include <hb-ot.h>
#include <hb.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::text {

std::atomic<std::uint64_t> FontFace::next_id_{1};

void FontFace::LibraryRelease::release(handle_type& handle) noexcept {
  FT_Done_FreeType(handle);
}

void FontFace::FaceRelease::release(handle_type& handle) noexcept {
  FT_Done_Face(handle);
}

void FontFace::BlobRelease::release(handle_type& handle) noexcept {
  hb_blob_destroy(handle);
}

void FontFace::ShapingFaceRelease::release(handle_type& handle) noexcept {
  hb_face_destroy(handle);
}

void FontFace::ShapingFontRelease::release(handle_type& handle) noexcept {
  hb_font_destroy(handle);
}

FontFace::FontFace() noexcept
    : raster_mutex_(std::make_unique<std::mutex>()),
      id_(next_id_.fetch_add(1)) {}

auto FontFace::create(std::shared_ptr<const provider::AssetBlob> bytes,
                      FaceOptions options)
    -> std::expected<FontFace, TextError> {
  if (bytes == nullptr) {
    return std::unexpected(TextError{TextErrorCode::invalid_font});
  }
  return open(Source{std::move(bytes)}, options);
}

auto FontFace::create(provider::FontData data, FaceOptions options)
    -> std::expected<FontFace, TextError> {
  return open(Source{data}, options);
}

auto FontFace::bytes(const Source& source) noexcept
    -> std::span<const std::byte> {
  if (const auto* blob =
          std::get_if<std::shared_ptr<const provider::AssetBlob>>(&source)) {
    return (*blob)->bytes();
  }
  return std::get<provider::FontData>(source).bytes();
}

auto FontFace::open(Source source, FaceOptions options)
    -> std::expected<FontFace, TextError> {
  const std::span<const std::byte> data = bytes(source);
  if (data.size() < 12 ||
      data.size() >
          static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
    return std::unexpected(TextError{TextErrorCode::invalid_font});
  }
  FontFace face;
  face.source_ = std::move(source);
  FT_Library library = nullptr;
  if (FT_Init_FreeType(&library) != 0) {
    return std::unexpected(TextError{TextErrorCode::font_engine});
  }
  face.library_ = support::UniqueHandle<LibraryRelease>{library};
  FT_Face native = nullptr;
  if (FT_New_Memory_Face(
          library,
          static_cast<const FT_Byte*>(static_cast<const void*>(data.data())),
          static_cast<FT_Long>(data.size()),
          static_cast<FT_Long>(options.index), &native) != 0) {
    return std::unexpected(TextError{TextErrorCode::invalid_font});
  }
  face.face_ = support::UniqueHandle<FaceRelease>{native};
  if ((native->face_flags & FT_FACE_FLAG_SFNT) == 0 ||
      native->units_per_EM == 0) {
    return std::unexpected(TextError{TextErrorCode::invalid_font});
  }
  face.blob_ = support::UniqueHandle<BlobRelease>{hb_blob_create(
      static_cast<const char*>(static_cast<const void*>(data.data())),
      static_cast<unsigned int>(data.size()), HB_MEMORY_MODE_READONLY, nullptr,
      nullptr)};
  face.shaping_face_ = support::UniqueHandle<ShapingFaceRelease>{
      hb_face_create(face.blob_.get(), options.index)};
  face.shaping_font_ = support::UniqueHandle<ShapingFontRelease>{
      hb_font_create(face.shaping_face_.get())};
  if (face.shaping_font_.get() == hb_font_get_empty() ||
      hb_face_get_upem(face.shaping_face_.get()) == 0) {
    return std::unexpected(TextError{TextErrorCode::font_engine});
  }
  face.units_per_em_ = native->units_per_EM;
  face.options_ = options;
  if (auto applied = face.apply_variations(options); !applied.has_value()) {
    return std::unexpected(applied.error());
  }
  face.read_metrics();
  face.apply_transform();
  hb_font_make_immutable(face.shaping_font_.get());
  return face;
}

auto FontFace::apply_variations(const FaceOptions& options)
    -> std::expected<void, TextError> {
  FT_Face native = face_.get();
  const auto* os2 =
      static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(native, FT_SFNT_OS2));
  if (os2 != nullptr) {
    if (auto declared = FontWeight::create(os2->usWeightClass)) {
      weight_ = *declared;
    }
  }
  if ((native->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS) == 0) {
    return {};
  }
  FT_MM_Var* axes = nullptr;
  if (FT_Get_MM_Var(native, &axes) != 0 || axes == nullptr) {
    return {};
  }
  constexpr std::uint32_t weight_tag =
      (std::uint32_t{'w'} << 24U) | (std::uint32_t{'g'} << 16U) |
      (std::uint32_t{'h'} << 8U) | std::uint32_t{'t'};
  constexpr std::uint32_t optical_tag =
      (std::uint32_t{'o'} << 24U) | (std::uint32_t{'p'} << 16U) |
      (std::uint32_t{'s'} << 8U) | std::uint32_t{'z'};
  constexpr float fixed_one = 65536.0F;
  std::vector<FT_Fixed> coordinates;
  std::vector<hb_variation_t> variations;
  coordinates.reserve(axes->num_axis);
  for (FT_UInt index = 0; index < axes->num_axis; ++index) {
    const FT_Var_Axis& axis = axes->axis[index];
    const float minimum = static_cast<float>(axis.minimum) / fixed_one;
    const float maximum = static_cast<float>(axis.maximum) / fixed_one;
    float value = static_cast<float>(axis.def) / fixed_one;
    const auto tag = static_cast<std::uint32_t>(axis.tag);
    if (tag == weight_tag && options.weight.has_value()) {
      value = static_cast<float>(options.weight->value());
    }
    if (tag == optical_tag && options.optical_size.has_value()) {
      value = *options.optical_size;
    }
    value = std::clamp(value, minimum, maximum);
    if (tag == weight_tag) {
      variable_ = true;
      if (auto applied = FontWeight::create(
              static_cast<std::uint16_t>(std::lround(value)))) {
        weight_ = *applied;
      }
    }
    coordinates.push_back(std::lround(value * fixed_one));
    variations.push_back(hb_variation_t{.tag = tag, .value = value});
  }
  const FT_UInt count = axes->num_axis;
  FT_Done_MM_Var(library_.get(), axes);
  if (FT_Set_Var_Design_Coordinates(native, count, coordinates.data()) != 0) {
    return std::unexpected(TextError{TextErrorCode::font_engine});
  }
  hb_font_set_variations(shaping_font_.get(), variations.data(),
                         static_cast<unsigned int>(variations.size()));
  return {};
}

void FontFace::read_metrics() noexcept {
  FT_Face native = face_.get();
  ascender_ = native->ascender;
  descender_ = native->descender;
  line_gap_ =
      std::max(0, native->height - (native->ascender - native->descender));
  const auto em = static_cast<std::int32_t>(units_per_em_);
  underline_position_ =
      native->underline_position != 0 ? native->underline_position : -(em / 10);
  underline_thickness_ = native->underline_thickness > 0
                             ? native->underline_thickness
                             : std::max(1, em / 14);
  strikeout_position_ = (ascender_ * 3) / 10;
  strikeout_thickness_ = underline_thickness_;
  const auto* os2 =
      static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(native, FT_SFNT_OS2));
  if (os2 != nullptr && os2->yStrikeoutSize > 0) {
    strikeout_position_ = os2->yStrikeoutPosition;
    strikeout_thickness_ = os2->yStrikeoutSize;
  }
}

void FontFace::apply_transform() noexcept {
  if (options_.stretch == 1.0F && options_.slant == 0.0F) {
    return;
  }
  constexpr float fixed_one = 65536.0F;
  FT_Matrix matrix{.xx = std::lround(options_.stretch * fixed_one),
                   .xy = std::lround(options_.slant * fixed_one),
                   .yx = 0,
                   .yy = std::lround(fixed_one)};
  FT_Set_Transform(face_.get(), &matrix, nullptr);
}

auto FontFace::family() const noexcept -> std::string_view {
  const char* name = face_.get()->family_name;
  return name == nullptr ? std::string_view{} : std::string_view{name};
}

auto FontFace::instance(FaceOptions options) const
    -> std::expected<FontFace, TextError> {
  return open(source_, options);
}

auto FontFace::glyph(char32_t codepoint) const noexcept
    -> std::optional<GlyphId> {
  hb_codepoint_t glyph = 0;
  if (hb_font_get_nominal_glyph(shaping_font_.get(), codepoint, &glyph) == 0) {
    return std::nullopt;
  }
  return GlyphId{glyph};
}

auto FontFace::covers(char32_t codepoint) const noexcept -> bool {
  const std::optional<GlyphId> found = glyph(codepoint);
  return found.has_value() && found->value() != 0;
}

auto FontFace::glyph_name(GlyphId glyph) const -> std::optional<std::string> {
  std::array<char, 128> name{};
  if (hb_font_get_glyph_name(shaping_font_.get(), glyph.value(), name.data(),
                             static_cast<unsigned int>(name.size())) == 0) {
    return std::nullopt;
  }
  return std::string{name.data()};
}

auto FontFace::shares_data_with(const FontFace& other) const noexcept -> bool {
  return bytes(source_).data() == bytes(other.source_).data();
}

auto FontFace::metrics(PointSize size) const noexcept -> FaceMetrics {
  const float scale = size.value() / static_cast<float>(units_per_em_);
  return FaceMetrics{
      .ascent = static_cast<float>(ascender_) * scale,
      .descent = static_cast<float>(-descender_) * scale,
      .line_gap = static_cast<float>(line_gap_) * scale,
      .underline_position = static_cast<float>(-underline_position_) * scale,
      .underline_thickness = static_cast<float>(underline_thickness_) * scale,
      .strikeout_position = static_cast<float>(strikeout_position_) * scale,
      .strikeout_thickness = static_cast<float>(strikeout_thickness_) * scale};
}

auto FontFace::rasterise(GlyphId glyph, float pixel_size,
                         float subpixel_x) const
    -> std::expected<GlyphBitmap, TextError> {
  if (!std::isfinite(pixel_size) || pixel_size <= 0.0F ||
      !std::isfinite(subpixel_x)) {
    return std::unexpected(TextError{TextErrorCode::invalid_argument});
  }
  const std::scoped_lock lock{*raster_mutex_};
  FT_Face native = face_.get();
  if (FT_Set_Char_Size(native, 0, std::lround(pixel_size * 64.0F), 72, 72) !=
      0) {
    return std::unexpected(TextError{TextErrorCode::font_engine});
  }
  constexpr FT_Int32 load_flags = FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP;
  if (FT_Load_Glyph(native, glyph.value(), load_flags) != 0) {
    return std::unexpected(TextError{TextErrorCode::font_engine});
  }
  FT_GlyphSlot slot = native->glyph;
  FT_Outline_Translate(&slot->outline, std::lround(subpixel_x * 64.0F), 0);
  if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) {
    return std::unexpected(TextError{TextErrorCode::font_engine});
  }
  const FT_Bitmap& bitmap = slot->bitmap;
  if (bitmap.width == 0 || bitmap.rows == 0 || bitmap.buffer == nullptr ||
      bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
    return GlyphBitmap{};
  }
  const std::size_t width = bitmap.width;
  const std::size_t rows = bitmap.rows;
  const auto pitch = static_cast<std::size_t>(std::abs(bitmap.pitch));
  std::vector<std::byte> coverage(width * rows);
  for (std::size_t row = 0; row < rows; ++row) {
    const unsigned char* source = bitmap.buffer + (row * pitch);
    std::ranges::transform(
        std::span{source, width},
        coverage.begin() + static_cast<std::ptrdiff_t>(row * width),
        [](unsigned char value) { return std::byte{value}; });
  }
  return GlyphBitmap{bitmap.width, bitmap.rows, slot->bitmap_left,
                     slot->bitmap_top, std::move(coverage)};
}

auto FontFace::Pen::at(const FT_Vector_* point) const -> composition::Point {
  const float across = static_cast<float>(point->x) * scale;
  const float up = static_cast<float>(point->y) * scale;
  return composition::Point{.x = x + (across * stretch) + (up * slant),
                            .y = y - up};
}

auto FontFace::move_to(const FT_Vector_* to, void* user) -> int {
  Pen& pen = *static_cast<Pen*>(user);
  if (pen.open) {
    pen.path.get().close_subpath();
  }
  pen.path.get().move(pen.at(to));
  pen.open = true;
  return 0;
}

auto FontFace::line_to(const FT_Vector_* to, void* user) -> int {
  Pen& pen = *static_cast<Pen*>(user);
  pen.path.get().add_line(pen.at(to));
  return 0;
}

auto FontFace::conic_to(const FT_Vector_* control, const FT_Vector_* to,
                        void* user) -> int {
  Pen& pen = *static_cast<Pen*>(user);
  pen.path.get().add_quad_curve(pen.at(to), pen.at(control));
  return 0;
}

auto FontFace::cubic_to(const FT_Vector_* first, const FT_Vector_* second,
                        const FT_Vector_* to, void* user) -> int {
  Pen& pen = *static_cast<Pen*>(user);
  pen.path.get().add_curve(pen.at(to), pen.at(first), pen.at(second));
  return 0;
}

auto FontFace::outline(std::string_view text, PointSize size) const
    -> std::expected<graphics::Path, TextError> {
  auto shaper = Shaper::create();
  if (!shaper.has_value()) {
    return std::unexpected(shaper.error());
  }
  const std::vector<ShapedGlyph> glyphs = shaper->shape(
      *this, ShapeRequest{.text = text,
                          .start = 0,
                          .end = static_cast<std::uint32_t>(text.size()),
                          .right_to_left = false,
                          .script = 0,
                          .language = {},
                          .size = size});
  graphics::Path path;
  const std::scoped_lock lock{*raster_mutex_};
  FT_Face native = face_.get();
  const float scale = size.value() / static_cast<float>(units_per_em_);
  const FT_Outline_Funcs funcs{.move_to = &FontFace::move_to,
                               .line_to = &FontFace::line_to,
                               .conic_to = &FontFace::conic_to,
                               .cubic_to = &FontFace::cubic_to,
                               .shift = 0,
                               .delta = 0};
  float advance = 0.0F;
  for (const ShapedGlyph& glyph : glyphs) {
    if (FT_Load_Glyph(
            native, glyph.glyph,
            FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0) {
      return std::unexpected(TextError{TextErrorCode::font_engine});
    }
    Pen pen{.path = path,
            .scale = scale,
            .stretch = options_.stretch,
            .slant = options_.slant,
            .x = advance + glyph.x_offset,
            .y = -glyph.y_offset};
    if (FT_Outline_Decompose(&native->glyph->outline, &funcs, &pen) != 0) {
      return std::unexpected(TextError{TextErrorCode::font_engine});
    }
    if (pen.open) {
      path.close_subpath();
    }
    advance += glyph.advance;
  }
  return path;
}

}
