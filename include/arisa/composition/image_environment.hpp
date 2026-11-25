#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/image_attributes.hpp>
#include <arisa/composition/symbol_variants.hpp>

#include <optional>

namespace arisa::composition {

class ImageEnvironment {
 public:
  static constexpr float small_scale = 0.8F;
  static constexpr float large_scale = 1.26F;

  static const EnvironmentKey<ImageScale> image_scale;
  static const EnvironmentKey<SymbolVariants> symbol_variants;
  static const EnvironmentKey<SymbolRenderingMode> symbol_rendering_mode;
  static const EnvironmentKey<std::optional<SymbolColorRenderingMode>>
      symbol_color_rendering_mode;
  static const EnvironmentKey<std::optional<SymbolVariableValueMode>>
      symbol_variable_value_mode;
  static const EnvironmentKey<std::optional<double>> symbol_layer_phase;
  static const EnvironmentKey<bool> symbol_effects_removed;
  static const EnvironmentKey<std::optional<ImageDynamicRange>>
      allowed_dynamic_range;

  static constexpr auto factor(ImageScale scale) noexcept -> float {
    switch (scale) {
      case ImageScale::small:
        return small_scale;
      case ImageScale::medium:
        return 1.0F;
      case ImageScale::large:
        return large_scale;
    }
    return 1.0F;
  }
};

}
