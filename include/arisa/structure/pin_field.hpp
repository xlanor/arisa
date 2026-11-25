#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/structure/digit_keypad.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::structure {

class PinField : public composition::Element {
 public:
  class Content;

  struct Options {
    bool secure{false};
    bool keypad{true};
    std::function<void(const std::string&)> complete;
    DigitKeypad::Options keys{};
  };

  static constexpr float box_width = 44.0F;
  static constexpr float box_height = 52.0F;
  static constexpr float box_spacing = 8.0F;
  static constexpr float box_radius = 10.0F;
  static constexpr float box_border = 2.0F;
  static constexpr float digit_size = 24.0F;
  static constexpr float spacing = 20.0F;
  static constexpr std::string_view mask = "•";
  static constexpr std::string_view box_prefix = "arisa.pin.box";

  PinField(std::size_t length, composition::Binding<std::string> code);

  PinField(std::size_t length, composition::Binding<std::string> code,
           Options options);

  static auto box_key(std::size_t index) -> std::string;

  static auto entered(std::string current, char digit, std::size_t length)
      -> std::string;

  static auto erased(std::string current) -> std::string;
};

class PinField::Content final : public composition::ElementContent {
 public:
  Content(std::size_t length, composition::Binding<std::string> code,
          Options options) noexcept
      : length_(length), code_(std::move(code)), options_(std::move(options)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  auto box(std::size_t index, const std::string& code) const
      -> composition::Element;

  auto keypad() const -> composition::Element;

  std::size_t length_;
  composition::Binding<std::string> code_;
  Options options_;
};

}
