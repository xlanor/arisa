#include <arisa/composition/bar_placement.hpp>

namespace arisa::composition {

const EnvironmentKey<std::optional<BarPlacement>> BarPlacements::header{
    "scaffold_header_placement", std::nullopt};
const EnvironmentKey<std::optional<BarPlacement>> BarPlacements::footer{
    "scaffold_footer_placement", std::nullopt};

}
