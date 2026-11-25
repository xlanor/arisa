#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace arisa::examples {

class GalleryWallpaper final : public composition::Component {
 public:
  enum class Look : std::uint8_t {
    launch,
    busy,
    window,
  };

  static constexpr composition::StateKey<bool> drifted{"wallpaper-drifted",
                                                       false};

  static constexpr std::array<std::uint32_t, 9> window_light{
      0xE8C5A3, 0xEBD5BE, 0xECE0D3, 0xF4D9A8, 0xEBDCD1,
      0xD9D3D2, 0xCBBAB2, 0xBDCCE6, 0xAFC0D8};

  static constexpr std::array<std::uint32_t, 9> window_dark{
      0x2A2744, 0x22203C, 0x1A1A2E, 0x312D52, 0x292748,
      0x3E4A70, 0x1C1B32, 0x23274C, 0x46587F};

  explicit GalleryWallpaper(Look look) noexcept : look_(look) {}

  auto operator==(const GalleryWallpaper&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto points() -> std::vector<composition::Point>;

  static auto band_points(bool drifting) -> std::vector<composition::Point>;

  static auto launch(composition::StateHandle<bool> drift)
      -> composition::Element;

  static auto busy() -> composition::Element;

  static auto window(bool dark) -> composition::Element;

  Look look_;
};

}
