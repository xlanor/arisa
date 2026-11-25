#include <arisa/composition/container_background_placement.hpp>

namespace arisa::composition {

const ContainerBackgroundPlacement::Key
    ContainerBackgroundPlacement::navigation_key{
        "container_background.navigation", std::nullopt};
const ContainerBackgroundPlacement::Key
    ContainerBackgroundPlacement::navigation_split_view_key{
        "container_background.navigation_split_view", std::nullopt};
const ContainerBackgroundPlacement::Key
    ContainerBackgroundPlacement::scaffold_key{"container_background.scaffold",
                                               std::nullopt};
const ContainerBackgroundPlacement::Key ContainerBackgroundPlacement::sheet_key{
    "container_background.sheet", std::nullopt};
const ContainerBackgroundPlacement::Key
    ContainerBackgroundPlacement::window_key{"container_background.window",
                                             std::nullopt};

auto ContainerBackgroundPlacement::key() const noexcept -> const Key& {
  switch (kind_) {
    case Kind::navigation:
      return navigation_key;
    case Kind::navigation_split_view:
      return navigation_split_view_key;
    case Kind::scaffold:
      return scaffold_key;
    case Kind::sheet:
      return sheet_key;
    case Kind::window:
      return window_key;
  }
  return window_key;
}

}
