#include <arisa/composition/image_environment.hpp>

namespace arisa::composition {

const EnvironmentKey<ImageScale> ImageEnvironment::image_scale{
    "image_scale", ImageScale::medium};
const EnvironmentKey<SymbolVariants> ImageEnvironment::symbol_variants{
    "symbol_variants", SymbolVariants::none};
const EnvironmentKey<SymbolRenderingMode>
    ImageEnvironment::symbol_rendering_mode{"symbol_rendering_mode",
                                            SymbolRenderingMode::monochrome};
const EnvironmentKey<std::optional<SymbolColorRenderingMode>>
    ImageEnvironment::symbol_color_rendering_mode{"symbol_color_rendering_mode",
                                                  std::nullopt};
const EnvironmentKey<std::optional<SymbolVariableValueMode>>
    ImageEnvironment::symbol_variable_value_mode{"symbol_variable_value_mode",
                                                 std::nullopt};
const EnvironmentKey<std::optional<double>>
    ImageEnvironment::symbol_layer_phase{"symbol_layer_phase", std::nullopt};
const EnvironmentKey<bool> ImageEnvironment::symbol_effects_removed{
    "symbol_effects_removed", false};
const EnvironmentKey<std::optional<ImageDynamicRange>>
    ImageEnvironment::allowed_dynamic_range{"allowed_dynamic_range",
                                            std::nullopt};

}
