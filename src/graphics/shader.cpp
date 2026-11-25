#include <arisa/graphics/shader.hpp>

#include <algorithm>
#include <bit>
#include <cstring>

namespace arisa::graphics {

ShaderFunction::ShaderFunction(const ShaderLibrary& library,
                               std::string_view name)
    : ShaderFunction(library[name].value_or(
          ShaderFunction{missing, Kind::colour, library.name(), {}})) {}

auto ShaderLibrary::default_library() -> ShaderLibrary {
  const std::span<const ShaderFunction> all = registered();
  return ShaderLibrary{"default", {all.begin(), all.end()}};
}

auto ShaderLibrary::bundle(std::string_view library) -> ShaderLibrary {
  std::vector<ShaderFunction> found;
  std::string_view name;
  for (const ShaderFunction& function : registered()) {
    if (function.library() == library) {
      found.push_back(function);
      name = function.library();
    }
  }
  return ShaderLibrary{name, std::move(found)};
}

auto ShaderLibrary::operator[](std::string_view name) const
    -> std::optional<ShaderFunction> {
  const auto found = std::ranges::find(functions_, name, &ShaderFunction::name);
  if (found == functions_.end()) {
    return std::nullopt;
  }
  return *found;
}

auto Shader::Argument::float_value(float value) -> Argument {
  return Argument{std::vector<float>{value}};
}

auto Shader::Argument::float2(float x, float y) -> Argument {
  return Argument{std::vector<float>{x, y}};
}

auto Shader::Argument::color(theme::Colour colour) -> Argument {
  return Argument{
      std::vector<float>{colour.red, colour.green, colour.blue, colour.alpha}};
}

auto Shader::Argument::float_array(std::span<const float> values) -> Argument {
  std::vector<float> packed{static_cast<float>(values.size())};
  packed.insert(packed.end(), values.begin(), values.end());
  return Argument{std::move(packed)};
}

auto Shader::Argument::color_array(std::span<const theme::Colour> colours)
    -> Argument {
  std::vector<float> packed{static_cast<float>(colours.size())};
  for (const theme::Colour& colour : colours) {
    packed.insert(packed.end(),
                  {colour.red, colour.green, colour.blue, colour.alpha});
  }
  return Argument{std::move(packed)};
}

auto Shader::Argument::data(std::span<const std::byte> bytes) -> Argument {
  std::vector<float> packed{static_cast<float>(bytes.size())};
  for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
    std::uint32_t word = 0;
    const std::size_t count = std::min<std::size_t>(4, bytes.size() - offset);
    for (std::size_t index = 0; index < count; ++index) {
      word |= static_cast<std::uint32_t>(bytes[offset + index]) << (8U * index);
    }
    packed.push_back(std::bit_cast<float>(word));
  }
  return Argument{std::move(packed)};
}

auto Shader::Argument::bounding_rect() -> Argument {
  return Argument{std::vector<float>(4, 0.0F), true};
}

auto Shader::argument(const Argument& value) && -> Shader {
  if (value.bounds()) {
    bounds_.push_back(arguments_.size());
  }
  arguments_.insert(arguments_.end(), value.values().begin(),
                    value.values().end());
  trim();
  return std::move(*this);
}

auto Shader::compile(UsageType usage) const -> std::expected<void, UsageError> {
  if (!function_.found()) {
    return std::unexpected(
        UsageError{.usage = usage, .kind = function_.kind(), .missing = true});
  }
  const ShaderFunction::Kind wanted =
      usage == UsageType::distortion_effect ? ShaderFunction::Kind::distortion
      : usage == UsageType::layer_effect    ? ShaderFunction::Kind::layer
                                            : ShaderFunction::Kind::colour;
  if (function_.kind() != wanted) {
    return std::unexpected(
        UsageError{.usage = usage, .kind = function_.kind(), .missing = false});
  }
  return {};
}

}
