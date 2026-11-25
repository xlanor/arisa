#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/colour_value.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace arisa::structure {

class CalloutBoard : public composition::Element {
 public:
  enum class Side : std::uint8_t {
    leading,
    trailing,
  };

  struct Callout {
    composition::Element chip;
    motion::UnitPoint anchor;
    Side side{Side::leading};
  };

  struct Options {
    composition::Size art{.width = 320.0F, .height = 240.0F};
    float column_width{180.0F};
    float gap{36.0F};
    float chip_height{38.0F};
    float chip_spacing{8.0F};
    float elbow{16.0F};
    float line_width{1.0F};
    float dot_diameter{7.0F};
    theme::ColourValue line{theme::ColourValue(theme::ColourRole::label).opacity(0.35F)};
  };

  struct Leader {
    composition::Point from;
    composition::Point elbow;
    composition::Point to;
  };

  static constexpr float art_margin = 32.0F;
  static constexpr float column_margin = 16.0F;

  static constexpr std::string_view art_key = "arisa.callout.art";
  static constexpr std::string_view lines_key = "arisa.callout.lines";
  static constexpr std::string_view chip_key = "arisa.callout.chip";

  CalloutBoard(composition::Element art, std::vector<Callout> callouts);

  CalloutBoard(composition::Element art, std::vector<Callout> callouts,
               Options options);

  static auto leaders(const std::vector<motion::UnitPoint>& anchors,
                      const std::vector<Side>& sides, const Options& options)
      -> std::vector<Leader>;

  static auto width(const Options& options) noexcept -> float;

  static auto height(const std::vector<Side>& sides,
                     const Options& options) noexcept -> float;

 private:
  static auto compose(composition::Element art, std::vector<Callout> callouts,
                      const Options& options) -> composition::Element;
};

}
