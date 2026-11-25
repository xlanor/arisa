#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <compare>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::examples {

class NavigationPage final : public composition::Component {
 public:
  struct Chapter {
    int level{1};
    std::string name;

    friend auto operator==(const Chapter&, const Chapter&) -> bool = default;
  };

  class Detail;

  static constexpr int deepest = 3;
  static constexpr std::string_view jump_button = "navigation-jump";
  static constexpr std::string_view root_list = "navigation-list";
  static const composition::StateKey<std::vector<Chapter>> path;

  auto operator==(const NavigationPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto chapter(int level, std::string_view name) -> Chapter;

 private:
  static auto library(composition::StateHandle<std::vector<Chapter>> chapters)
      -> composition::Element;
};

class NavigationPage::Detail final : public composition::Component {
 public:
  static constexpr composition::StateKey<int> taps{"navigation-taps", 0};
  static constexpr std::string_view tap_button = "navigation-tap";
  static constexpr std::string_view deeper_link = "navigation-deeper";
  static constexpr std::string_view root_button = "navigation-root";

  Detail(Chapter chapter,
         composition::StateHandle<std::vector<Chapter>> chapters)
      : chapter_(std::move(chapter)), chapters_(std::move(chapters)) {}

  auto operator==(const Detail& other) const -> bool {
    return chapter_ == other.chapter_ &&
           chapters_.identity() == other.chapters_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  Chapter chapter_;
  composition::StateHandle<std::vector<Chapter>> chapters_;
};

}
