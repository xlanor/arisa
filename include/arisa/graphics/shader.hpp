#pragma once

#include <arisa/composition/size.hpp>
#include <arisa/theme/colour.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::graphics {

class ShaderLibrary;

class ShaderFunction {
 public:
  enum class Kind : std::uint8_t { colour, distortion, layer };

  static constexpr std::uint32_t missing =
      std::numeric_limits<std::uint32_t>::max();

  constexpr ShaderFunction(std::uint32_t index, Kind kind) noexcept
      : index_(index), kind_(kind) {}

  constexpr ShaderFunction(std::uint32_t index, Kind kind,
                           std::string_view library,
                           std::string_view name) noexcept
      : index_(index), kind_(kind), library_(library), name_(name) {}

  ShaderFunction(const ShaderLibrary& library, std::string_view name);

  constexpr auto index() const noexcept -> std::uint32_t { return index_; }

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto library() const noexcept -> std::string_view {
    return library_;
  }

  constexpr auto name() const noexcept -> std::string_view { return name_; }

  constexpr auto found() const noexcept -> bool { return index_ != missing; }

  friend constexpr auto operator==(const ShaderFunction& left,
                                   const ShaderFunction& right) noexcept
      -> bool {
    return left.index_ == right.index_ && left.kind_ == right.kind_;
  }

 private:
  std::uint32_t index_;
  Kind kind_;
  std::string_view library_{};
  std::string_view name_{};
};

class ShaderLibrary {
 public:
  static auto default_library() -> ShaderLibrary;

  static auto bundle(std::string_view library) -> ShaderLibrary;

  auto operator[](std::string_view name) const -> std::optional<ShaderFunction>;

  auto functions() const noexcept -> std::span<const ShaderFunction> {
    return functions_;
  }

  auto name() const noexcept -> std::string_view { return name_; }

  friend auto operator==(const ShaderLibrary& left, const ShaderLibrary& right)
      -> bool {
    return left.name_ == right.name_ && left.functions_ == right.functions_;
  }

 private:
  ShaderLibrary(std::string_view name, std::vector<ShaderFunction> functions)
      : name_(name), functions_(std::move(functions)) {}

  static auto registered() -> std::span<const ShaderFunction>;

  std::string_view name_;
  std::vector<ShaderFunction> functions_;
};

class Shader {
 public:
  enum class UsageType : std::uint8_t {
    color_effect,
    distortion_effect,
    layer_effect,
    shape_style,
  };

  struct UsageError {
    UsageType usage{UsageType::color_effect};
    ShaderFunction::Kind kind{ShaderFunction::Kind::colour};
    bool missing{false};

    friend constexpr auto operator==(const UsageError&, const UsageError&)
        -> bool = default;
  };

  class Argument {
   public:
    static auto float_value(float value) -> Argument;

    static auto float2(float x, float y) -> Argument;

    static auto color(theme::Colour colour) -> Argument;

    static auto float_array(std::span<const float> values) -> Argument;

    static auto color_array(std::span<const theme::Colour> colours) -> Argument;

    static auto data(std::span<const std::byte> bytes) -> Argument;

    static auto bounding_rect() -> Argument;

    auto values() const noexcept -> const std::vector<float>& {
      return values_;
    }

    auto bounds() const noexcept -> bool { return bounds_; }

    friend auto operator==(const Argument&, const Argument&) -> bool = default;

   private:
    explicit Argument(std::vector<float> values, bool bounds = false)
        : values_(std::move(values)), bounds_(bounds) {}

    std::vector<float> values_;
    bool bounds_{false};
  };

  static constexpr std::size_t maximum_arguments = 247;

  explicit Shader(ShaderFunction function,
                  std::initializer_list<float> arguments = {})
      : function_(function), arguments_(arguments) {
    trim();
  }

  Shader(ShaderFunction function, std::vector<float> arguments)
      : function_(function), arguments_(std::move(arguments)) {
    trim();
  }

  auto argument(float value) && -> Shader {
    arguments_.push_back(value);
    trim();
    return std::move(*this);
  }

  auto argument(float x, float y) && -> Shader {
    arguments_.push_back(x);
    arguments_.push_back(y);
    trim();
    return std::move(*this);
  }

  auto argument(theme::Colour colour) && -> Shader {
    arguments_.insert(arguments_.end(),
                      {colour.red, colour.green, colour.blue, colour.alpha});
    trim();
    return std::move(*this);
  }

  auto argument(const Argument& value) && -> Shader;

  auto dithers_color(bool dithers) && -> Shader {
    dithers_ = dithers;
    return std::move(*this);
  }

  auto dithers_color() const noexcept -> bool { return dithers_; }

  [[nodiscard]] auto compile(UsageType usage) const
      -> std::expected<void, UsageError>;

  auto function() const noexcept -> ShaderFunction { return function_; }

  auto arguments() const noexcept -> const std::vector<float>& {
    return arguments_;
  }

  auto bounds_slots() const noexcept -> const std::vector<std::size_t>& {
    return bounds_;
  }

  friend auto operator==(const Shader&, const Shader&) -> bool = default;

 private:
  void trim() {
    if (arguments_.size() > maximum_arguments) {
      arguments_.resize(maximum_arguments);
    }
    std::erase_if(bounds_, [this](std::size_t slot) {
      return slot + 4 > arguments_.size();
    });
  }

  ShaderFunction function_;
  std::vector<float> arguments_;
  std::vector<std::size_t> bounds_;
  bool dithers_{false};
};

}
