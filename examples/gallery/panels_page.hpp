#pragma once

#include <arisa/components/panel_radius.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace arisa::examples {

class PanelsPage final : public composition::Component {
 public:
  static constexpr std::size_t steps = 4;
  static constexpr float panel_width = 132.0F;
  static constexpr float form_height = 360.0F;
  static constexpr composition::StateKey<std::size_t> dock_selection{
      "panels-dock", 0};
  static constexpr composition::StateKey<std::size_t> step{"panels-step", 1};
  static constexpr composition::StateKey<int> opened{"panels-opened", 0};
  static constexpr composition::StateKey<bool> stats{"panels-stats", true};
  static constexpr composition::StateKey<bool> haptics{"panels-haptics", false};
  static constexpr composition::StateKey<std::string> passcode{"panels-pin",
                                                               ""};
  static constexpr std::string_view form_key = "panels-form";
  static constexpr std::string_view dock_key = "panels-dock";
  static constexpr std::string_view compact_dock_key = "panels-dock-compact";
  static constexpr std::string_view rail_key = "panels-rail";
  static constexpr std::string_view small_dock_key = "panels-dock-small";
  static constexpr std::string_view disclosure_key = "panels-disclosure";
  static constexpr std::string_view callout_key = "panels-callouts";
  static constexpr std::string_view qr_key = "panels-qr";
  static constexpr std::string_view pin_key = "panels-pin";
  static constexpr std::string_view tiles_key = "panels-tiles";
  static constexpr float qr_extent = 168.0F;
  static constexpr std::size_t pin_length = 4;
  static constexpr float rail_height = 260.0F;
  static constexpr std::string_view progress_key = "panels-progress";
  static constexpr std::string_view advance_key = "panels-advance";
  static constexpr std::string_view help_key = "panels-help";
  static constexpr std::string_view help_title = "Stream quality";
  static constexpr std::string_view help_body =
      "Higher resolutions need a faster network. Lower it if the picture "
      "stutters.";

  auto operator==(const PanelsPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto panel_key(components::PanelRadius radius) -> std::string;

 private:
  static auto card(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto panels() -> composition::Element;

  static auto form(composition::StateHandle<bool> shown,
                   composition::StateHandle<bool> felt) -> composition::Element;

  static auto progress(composition::StateHandle<std::size_t> current)
      -> composition::Element;

  static auto dock(composition::StateHandle<std::size_t> selection,
                   composition::StateHandle<int> count) -> composition::Element;

  static auto rails(composition::StateHandle<std::size_t> selection)
      -> composition::Element;

  static auto callouts() -> composition::Element;

  static auto qr_code() -> composition::Element;

  static auto pin(composition::StateHandle<std::string> code)
      -> composition::Element;

  static auto tiles() -> composition::Element;
};

}
