#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/graphics/path.hpp>

#include <string_view>
#include <vector>

namespace arisa::examples {

class PathsPage final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> writing{"paths-writing", false};
  static constexpr composition::StateKey<bool> chasing{"paths-chasing", false};
  static constexpr composition::StateKey<bool> drifting{"paths-drifting",
                                                        false};
  static constexpr std::string_view trim_view = "paths-trim";
  static constexpr std::string_view chase_view = "paths-chase";
  static constexpr std::string_view mesh_view = "paths-mesh";
  static constexpr float tile_width = 120.0F;
  static constexpr float tile_height = 84.0F;

  auto operator==(const PathsPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto star(float size) -> graphics::Path;

  static auto mesh_points(bool drifted) -> std::vector<composition::Point>;

 private:
  static auto section(std::string_view title,
                      std::vector<composition::Element> tiles)
      -> composition::Element;

  static auto tile(composition::Element content, std::string_view caption)
      -> composition::Element;

  static auto shapes() -> composition::Element;

  static auto strokes() -> composition::Element;

  static auto gradients() -> composition::Element;

  static auto trims(composition::StateHandle<bool> write,
                    composition::StateHandle<bool> chase)
      -> composition::Element;

  static auto mesh(composition::StateHandle<bool> drift)
      -> composition::Element;

  static auto delay_chase(composition::StateHandle<bool> chase)
      -> async::Task<void>;
};

}
