#include <arisa/components/hstack.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/structure/pin_field.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <memory>
#include <utility>

namespace arisa::structure {

PinField::PinField(std::size_t length, composition::Binding<std::string> code)
    : PinField(length, std::move(code), Options{}) {}

PinField::PinField(std::size_t length, composition::Binding<std::string> code,
                   Options options)
    : Element(std::make_unique<Content>(length, std::move(code),
                                        std::move(options))) {}

auto PinField::box_key(std::size_t index) -> std::string {
  return std::format("{}.{}", box_prefix, index);
}

auto PinField::entered(std::string current, char digit, std::size_t length)
    -> std::string {
  if (digit >= '0' && digit <= '9' && current.size() < length) {
    current.push_back(digit);
  }
  return current;
}

auto PinField::erased(std::string current) -> std::string {
  if (!current.empty()) {
    current.pop_back();
  }
  return current;
}

auto PinField::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto PinField::Content::box(std::size_t index, const std::string& code) const
    -> composition::Element {
  const bool filled = index < code.size();
  const bool next = index == code.size();
  const theme::Shape shape = theme::Shape::rounded(box_radius, true);
  std::string shown;
  if (filled) {
    shown = options_.secure ? std::string{mask} : std::string(1, code[index]);
  }
  composition::Element made =
      components::ZStack(
          components::Text(std::move(shown))
              .font(theme::Font::system(digit_size,
                                        theme::Font::Weight::semibold))
              .monospaced_digit())
          .frame({.width = box_width, .height = box_height})
          .background(theme::ColourRole::secondary_system_fill, shape);
  if (next) {
    made = std::move(made).overlay(
        components::ShapeView(shape).stroke_border(theme::ColourRole::accent,
                                                   box_border));
  }
  return std::move(made).id(PinField::box_key(index));
}

auto PinField::Content::keypad() const -> composition::Element {
  const std::size_t length = length_;
  const composition::Binding<std::string> code = code_;
  const std::function<void(const std::string&)> complete = options_.complete;
  DigitKeypad::Actions actions{
      .digit =
          [code, length, complete](char digit) {
            const std::string before = code.get();
            std::string after = entered(before, digit, length);
            if (after == before) {
              return;
            }
            code.set(after);
            if (after.size() == length && complete) {
              complete(after);
            }
          },
      .erase = [code] { code.set(erased(code.get())); },
      .submit = nullptr};
  if (complete) {
    actions.submit = [code, length, complete] {
      const std::string value = code.get();
      if (value.size() == length) {
        complete(value);
      }
    };
  }
  return DigitKeypad(std::move(actions), options_.keys);
}

auto PinField::Content::body(composition::Context&) const
    -> std::vector<composition::Element> {
  const std::string code = code_.get();
  std::vector<composition::Element> boxes;
  boxes.reserve(length_);
  for (std::size_t index = 0; index < length_; ++index) {
    boxes.push_back(box(index, code));
  }
  std::vector<composition::Element> parts;
  parts.push_back(components::HStack(
                      components::HStackOptions{
                          .spacing = theme::Space{box_spacing}},
                      std::move(boxes)));
  if (options_.keypad) {
    parts.push_back(keypad());
  }
  std::vector<composition::Element> produced;
  produced.push_back(components::VStack(
      components::VStackOptions{.spacing = theme::Space{spacing}},
      std::move(parts)));
  return produced;
}

}
