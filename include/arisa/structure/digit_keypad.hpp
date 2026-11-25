#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::structure {

class DigitKeypad : public composition::Element {
 public:
  class Content;

  struct Actions {
    std::function<void(char)> digit;
    std::function<void()> erase;
    std::function<void()> submit;
  };

  struct Options {
    std::string erase_symbol{"arrow_back"};
    std::string submit_symbol{"check"};
  };

  static constexpr float key_width = 72.0F;
  static constexpr float key_height = 52.0F;
  static constexpr float key_spacing = 8.0F;
  static constexpr float key_radius = 14.0F;
  static constexpr float digit_size = 24.0F;
  static constexpr float symbol_size = 20.0F;
  static constexpr std::string_view key_prefix = "arisa.keypad";
  static constexpr std::string_view erase_name = "erase";
  static constexpr std::string_view submit_name = "submit";

  explicit DigitKeypad(Actions actions);

  DigitKeypad(Actions actions, Options options);

  static auto key_of(char digit) -> std::string;

  static auto key_of(std::string_view name) -> std::string;
};

class DigitKeypad::Content final : public composition::ElementContent {
 public:
  Content(Actions actions, Options options) noexcept
      : actions_(std::move(actions)), options_(std::move(options)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  static auto key(composition::Element label, std::function<void()> action,
                  std::string id) -> composition::Element;

  auto digit(char value) const -> composition::Element;

  auto symbol(const std::string& name, const std::function<void()>& action,
              std::string_view id) const -> composition::Element;

  Actions actions_;
  Options options_;
};

}
