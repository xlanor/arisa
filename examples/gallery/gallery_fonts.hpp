#pragma once

#include <arisa/provider/assets/provider.hpp>
#include <arisa/text/font_entry.hpp>
#include <arisa/text/font_face.hpp>
#include <arisa/text/font_role.hpp>

#include <array>
#include <memory>
#include <string_view>
#include <vector>

namespace arisa::examples {

class GalleryFonts {
 public:
  struct Candidate {
    std::string_view path;
    text::FontRole role;
  };

  static constexpr std::array candidates{
      Candidate{"arisa/fonts/NotoSansJP-Subset.otf", text::FontRole::japanese},
      Candidate{"arisa/fonts/NotoSansSC-Subset.otf",
                text::FontRole::chinese_simplified},
      Candidate{"arisa/fonts/NotoSansTC-Subset.otf",
                text::FontRole::chinese_traditional},
      Candidate{"arisa/fonts/NotoSansKR-Subset.otf", text::FontRole::korean},
      Candidate{"arisa/fonts/NotoSansArabic-Regular.ttf",
                text::FontRole::script},
      Candidate{"arisa/fonts/NotoSansHebrew-Regular.ttf",
                text::FontRole::script},
      Candidate{"arisa/fonts/NotoSansThai-Subset.ttf", text::FontRole::script},
      Candidate{"arisa/fonts/NotoSansDevanagari-Subset.ttf",
                text::FontRole::script}};

  static constexpr std::string_view lettering_path = "arisa/fonts/Bumbbled.otf";
  static constexpr std::string_view lettering_name = "Bumbbled";

  static auto load(const provider::AssetProvider& assets)
      -> std::vector<text::FontEntry>;

  static auto lettering(const provider::AssetProvider& assets)
      -> std::shared_ptr<const text::FontFace>;
};

}
