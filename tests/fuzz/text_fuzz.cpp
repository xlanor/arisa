#include <arisa/provider/assets/asset_blob.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/headless/memory_fonts.hpp>
#include <arisa/provider/standard/directory_assets.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/text/font_collection.hpp>
#include <arisa/text/font_entry.hpp>
#include <arisa/text/font_face.hpp>
#include <arisa/text/font_role.hpp>
#include <arisa/text/layout_width.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/point_size.hpp>
#include <arisa/text/text_style.hpp>
#include <arisa/text/typesetter.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::text {

class TextFuzzer {
 public:
  static auto instance() -> TextFuzzer& {
    static TextFuzzer fuzzer;
    return fuzzer;
  }

  void run(std::span<const std::uint8_t> input) {
    if (input.size() < 3) {
      return;
    }
    TextStyle style;
    style.size = PointSize::create(4.0F + static_cast<float>(input[0] % 60))
                     .value_or(style.size);
    style.locale = Locale::create(locales[input[1] % locales.size()])
                       .value_or(Locale::undetermined());
    style.tracking = static_cast<float>(input[2] % 7) - 3.0F;
    const LayoutWidth width =
        input[1] % 3 == 0
            ? LayoutWidth::unbounded()
            : LayoutWidth::points(static_cast<float>(input[2]) * 2.0F)
                  .value_or(LayoutWidth::unbounded());
    const std::string_view text{
        reinterpret_cast<const char*>(input.subspan(3).data()),
        input.size() - 3};
    static_cast<void>(
        typesetter_.layout(text, style, width, provider::SurfaceScale::unit()));
    static_cast<void>(typesetter_.measure(text, style, width,
                                          provider::SurfaceScale::unit()));
    if (++runs_ % 512 == 0) {
      typesetter_.clear();
    }
  }

 private:
  static constexpr std::array<std::string_view, 8> locales{
      "en-US", "ar", "he", "th", "hi", "ja-JP", "zh-Hans", "ko"};

  static constexpr std::array<std::string_view, 4> scripts{
      "arisa/fonts/NotoSansArabic-Subset.ttf",
      "arisa/fonts/NotoSansHebrew-Subset.ttf",
      "arisa/fonts/NotoSansThai-Subset.ttf",
      "arisa/fonts/NotoSansDevanagari-Subset.ttf"};

  TextFuzzer()
      : fonts_(collection(system_)),
        typesetter_(std::move(Typesetter::create(fonts_).value())) {}

  static auto collection(provider::headless::MemoryFonts& system)
      -> FontCollection {
    const provider::standard::DirectoryAssets fixtures{
        std::filesystem::path{ARISA_TEST_FIXTURES}};
    if (auto japanese = fixtures.read(*provider::AssetPath::create(
            "arisa/fonts/NotoSansJP-Subset.otf"))) {
      system.add(provider::SystemFont::standard, japanese->bytes());
    }
    std::vector<FontEntry> app;
    for (const std::string_view path : scripts) {
      auto blob = fixtures.read(*provider::AssetPath::create(path));
      if (!blob.has_value()) {
        std::abort();
      }
      auto face = FontFace::create(
          std::make_shared<const provider::AssetBlob>(std::move(*blob)));
      if (!face.has_value()) {
        std::abort();
      }
      app.push_back(FontEntry{
          .role = FontRole::script,
          .face = std::make_shared<const FontFace>(std::move(*face))});
    }
    const provider::standard::DirectoryAssets assets{
        std::filesystem::path{ARISA_SOURCE_ASSETS}};
    auto loaded = FontCollection::load(assets, system, std::move(app));
    if (!loaded.has_value()) {
      std::abort();
    }
    return std::move(*loaded);
  }

  provider::headless::MemoryFonts system_;
  FontCollection fonts_;
  Typesetter typesetter_;
  std::size_t runs_{0};
};

}

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                       std::size_t size) -> int {
  arisa::text::TextFuzzer::instance().run(
      std::span<const std::uint8_t>{data, size});
  return 0;
}
