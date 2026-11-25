#pragma once

#include <arisa/composition/point.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/text/point_size.hpp>

#include <functional>
#include <string_view>

#include <arisa/provider/assets/asset_blob.hpp>
#include <arisa/provider/fonts/font_data.hpp>
#include <arisa/support/unique_handle.hpp>
#include <arisa/text/face_id.hpp>
#include <arisa/text/face_metrics.hpp>
#include <arisa/text/face_options.hpp>
#include <arisa/text/font_weight.hpp>
#include <arisa/text/glyph_bitmap.hpp>
#include <arisa/text/glyph_id.hpp>
#include <arisa/text/point_size.hpp>
#include <arisa/text/text_error.hpp>

#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <variant>

struct FT_LibraryRec_;
struct FT_FaceRec_;
struct hb_blob_t;
struct hb_face_t;
struct hb_font_t;

struct FT_Vector_;

namespace arisa::text {

class FontFace {
 public:
  [[nodiscard]] static auto create(
      std::shared_ptr<const provider::AssetBlob> bytes,
      FaceOptions options = {}) -> std::expected<FontFace, TextError>;

  [[nodiscard]] static auto create(provider::FontData data,
                                   FaceOptions options = {})
      -> std::expected<FontFace, TextError>;

  auto id() const noexcept -> FaceId { return id_; }

  auto glyph(char32_t codepoint) const noexcept -> std::optional<GlyphId>;

  auto covers(char32_t codepoint) const noexcept -> bool;

  auto glyph_name(GlyphId glyph) const -> std::optional<std::string>;

  auto units_per_em() const noexcept -> std::uint32_t { return units_per_em_; }

  auto metrics(PointSize size) const noexcept -> FaceMetrics;

  auto weight() const noexcept -> FontWeight { return weight_; }

  auto variable() const noexcept -> bool { return variable_; }

  auto shares_data_with(const FontFace& other) const noexcept -> bool;

  auto options() const noexcept -> const FaceOptions& { return options_; }

  auto family() const noexcept -> std::string_view;

  [[nodiscard]] auto instance(FaceOptions options) const
      -> std::expected<FontFace, TextError>;

  [[nodiscard]] auto rasterise(GlyphId glyph, float pixel_size,
                               float subpixel_x) const
      -> std::expected<GlyphBitmap, TextError>;

  [[nodiscard]] auto outline(std::string_view text, PointSize size) const
      -> std::expected<graphics::Path, TextError>;

 private:
  friend class Shaper;

  struct LibraryRelease {
    using handle_type = FT_LibraryRec_*;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type handle) noexcept -> bool {
      return handle != nullptr;
    }
    static void release(handle_type& handle) noexcept;
  };

  struct FaceRelease {
    using handle_type = FT_FaceRec_*;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type handle) noexcept -> bool {
      return handle != nullptr;
    }
    static void release(handle_type& handle) noexcept;
  };

  struct BlobRelease {
    using handle_type = hb_blob_t*;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type handle) noexcept -> bool {
      return handle != nullptr;
    }
    static void release(handle_type& handle) noexcept;
  };

  struct ShapingFaceRelease {
    using handle_type = hb_face_t*;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type handle) noexcept -> bool {
      return handle != nullptr;
    }
    static void release(handle_type& handle) noexcept;
  };

  struct ShapingFontRelease {
    using handle_type = hb_font_t*;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type handle) noexcept -> bool {
      return handle != nullptr;
    }
    static void release(handle_type& handle) noexcept;
  };

  using Source = std::variant<std::shared_ptr<const provider::AssetBlob>,
                              provider::FontData>;

  FontFace() noexcept;

  [[nodiscard]] static auto open(Source source, FaceOptions options)
      -> std::expected<FontFace, TextError>;

  static auto bytes(const Source& source) noexcept
      -> std::span<const std::byte>;

  [[nodiscard]] auto apply_variations(const FaceOptions& options)
      -> std::expected<void, TextError>;

  void read_metrics() noexcept;

  void apply_transform() noexcept;

  struct Pen {
    std::reference_wrapper<graphics::Path> path;
    float scale{1.0F};
    float stretch{1.0F};
    float slant{0.0F};
    float x{0.0F};
    float y{0.0F};
    bool open{false};

    auto at(const FT_Vector_* point) const -> composition::Point;
  };

  static auto move_to(const FT_Vector_* to, void* user) -> int;
  static auto line_to(const FT_Vector_* to, void* user) -> int;
  static auto conic_to(const FT_Vector_* control, const FT_Vector_* to,
                       void* user) -> int;
  static auto cubic_to(const FT_Vector_* first, const FT_Vector_* second,
                       const FT_Vector_* to, void* user) -> int;

  auto shaping_font() const noexcept -> hb_font_t* {
    return shaping_font_.get();
  }

  static std::atomic<std::uint64_t> next_id_;

  Source source_;
  support::UniqueHandle<LibraryRelease> library_;
  support::UniqueHandle<FaceRelease> face_;
  support::UniqueHandle<BlobRelease> blob_;
  support::UniqueHandle<ShapingFaceRelease> shaping_face_;
  support::UniqueHandle<ShapingFontRelease> shaping_font_;
  std::unique_ptr<std::mutex> raster_mutex_;
  FaceId id_{0};
  std::uint32_t units_per_em_{0};
  FontWeight weight_{font_weight::regular};
  std::int32_t ascender_{0};
  std::int32_t descender_{0};
  std::int32_t line_gap_{0};
  std::int32_t underline_position_{0};
  std::int32_t underline_thickness_{0};
  std::int32_t strikeout_position_{0};
  std::int32_t strikeout_thickness_{0};
  FaceOptions options_;
  bool variable_{false};
};

}
