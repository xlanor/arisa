#include <gallery_fonts.hpp>

#include <arisa/provider/assets/asset_blob.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/text/font_face.hpp>

#include <memory>
#include <string>
#include <utility>

namespace arisa::examples {

auto GalleryFonts::load(const provider::AssetProvider& assets)
    -> std::vector<text::FontEntry> {
  std::vector<text::FontEntry> entries;
  for (const Candidate& candidate : candidates) {
    const auto path = provider::AssetPath::create(candidate.path);
    if (!path.has_value() || !assets.exists(*path)) {
      continue;
    }
    auto blob = assets.read(*path);
    if (!blob.has_value()) {
      continue;
    }
    auto face = text::FontFace::create(
        std::make_shared<const provider::AssetBlob>(std::move(*blob)));
    if (!face.has_value()) {
      continue;
    }
    entries.push_back(text::FontEntry{
        .role = candidate.role,
        .face = std::make_shared<const text::FontFace>(std::move(*face)),
        .design = text::FontDesign::text});
  }
  if (auto lettered = lettering(assets)) {
    entries.push_back(text::FontEntry{.role = text::FontRole::latin,
                                      .face = std::move(lettered),
                                      .design = text::FontDesign::text,
                                      .typeface = text::Typeface::standard,
                                      .italic = false,
                                      .name = std::string{lettering_name}});
  }
  return entries;
}

auto GalleryFonts::lettering(const provider::AssetProvider& assets)
    -> std::shared_ptr<const text::FontFace> {
  const auto path = provider::AssetPath::create(lettering_path);
  if (!path.has_value() || !assets.exists(*path)) {
    return nullptr;
  }
  auto blob = assets.read(*path);
  if (!blob.has_value()) {
    return nullptr;
  }
  auto face = text::FontFace::create(
      std::make_shared<const provider::AssetBlob>(std::move(*blob)));
  if (!face.has_value()) {
    return nullptr;
  }
  return std::make_shared<const text::FontFace>(std::move(*face));
}

}
