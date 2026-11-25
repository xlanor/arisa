#pragma once

#include <arisa/composition/app_storage.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/structure/tab_view_customization.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::examples {

class ChromePage final : public composition::Component {
 public:
  class Opener;

  enum class Demo : std::uint8_t {
    mail,
    onboarding,
    tabs,
    toolbar,
    bottom_bar,
    toolbar_items,
    tab_extras,
    search,
    dialogs,
  };

  static constexpr std::string_view close_button = "chrome-close";
  static constexpr std::string_view large_button = "chrome-large";
  static constexpr std::string_view mail_button = "chrome-mail";
  static constexpr std::string_view onboarding_button = "chrome-onboarding";
  static constexpr std::string_view tabs_button = "chrome-tabs";
  static constexpr std::string_view toolbar_button = "chrome-toolbar";
  static constexpr std::string_view bottom_button = "chrome-bottom";
  static constexpr std::string_view items_button = "chrome-items";
  static constexpr std::string_view extras_button = "chrome-extras";
  static constexpr std::string_view search_button = "chrome-search";
  static constexpr std::string_view dialogs_button = "chrome-dialogs";
  static constexpr std::string_view toolbar_id = "gallery.chrome.items";
  static constexpr int message_count = 12;
  static constexpr int row_count = 30;
  static constexpr std::array<std::string_view, 3> mailboxes{"Inbox", "Sent",
                                                             "Archive"};
  static constexpr composition::StateKey<bool> large_shown{"chrome-large",
                                                           false};
  static constexpr composition::StateKey<std::optional<Demo>> demo{
      "chrome-demo", std::nullopt};
  static constexpr composition::StateKey<int> mailbox{"chrome-mailbox", 0};
  static constexpr composition::StateKey<int> message{"chrome-message", 0};
  static constexpr composition::StateKey<int> page{"chrome-page", 0};
  static constexpr composition::StateKey<int> tab{"chrome-tab", 0};
  static constexpr composition::StateKey<int> taps{"chrome-taps", 0};
  static constexpr composition::StateKey<bool> editor{"chrome-editor", false};
  static constexpr composition::StateKey<bool> searching{"chrome-searching",
                                                         false};
  static constexpr composition::StateKey<std::string> query{"chrome-query",
                                                            std::string{}};
  static constexpr composition::StateKey<std::optional<int>> pending{
      "chrome-pending", std::nullopt};
  static constexpr composition::StateKey<std::optional<int>> failing{
      "chrome-failing", std::nullopt};
  static constexpr composition::StateKey<bool> placed{"chrome-placed", false};
  static const composition::AppStorage<structure::TabViewCustomization>
      tab_order;

  explicit ChromePage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const ChromePage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto title_of(Demo chosen) -> std::string_view;

 private:
  static auto tiles(composition::StateHandle<bool> large,
                    composition::StateHandle<std::optional<Demo>> chosen)
      -> composition::Element;

  static auto tile(std::string_view title, std::string_view symbol,
                   std::string_view key, std::function<void()> action)
      -> composition::Element;

  static auto large_title() -> composition::Element;

  struct Handles {
    composition::StateHandle<int> mailbox;
    composition::StateHandle<int> message;
    composition::StateHandle<int> page;
    composition::StateHandle<int> tab;
    composition::StateHandle<int> taps;
    composition::StateHandle<bool> editor;
    composition::Binding<structure::TabViewCustomization> order;
    composition::StateHandle<bool> searching;
    composition::StateHandle<std::string> query;
    composition::StateHandle<std::optional<int>> pending;
    composition::StateHandle<std::optional<int>> failing;
    composition::StateHandle<bool> placed;
  };

  class Failure {
   public:
    explicit Failure(int code) noexcept : code_(code) {}

    auto error_description() const -> std::string;

    auto code() const noexcept -> int { return code_; }

    friend auto operator==(const Failure&, const Failure&) -> bool = default;

   private:
    int code_;
  };

  static auto shown(const Handles& handles, Demo chosen)
      -> composition::Element;

  static auto mail(composition::StateHandle<int> box,
                   composition::StateHandle<int> letter)
      -> composition::Element;

  static auto onboarding(composition::StateHandle<int> step)
      -> composition::Element;

  static auto tabs(composition::StateHandle<int> selected,
                   composition::Binding<structure::TabViewCustomization> order)
      -> composition::Element;

  static auto toolbar(composition::StateHandle<int> count,
                      composition::StateHandle<bool> edits)
      -> composition::Element;

  static auto bottom_bar(composition::StateHandle<int> count)
      -> composition::Element;

  static auto toolbar_items(composition::StateHandle<int> count)
      -> composition::Element;

  static auto tab_extras(composition::StateHandle<int> selected)
      -> composition::Element;

  static auto search(composition::StateHandle<std::string> text,
                     composition::StateHandle<bool> active)
      -> composition::Element;

  static auto dialogs(const Handles& handles) -> composition::Element;

  static auto rows(std::string_view prefix, int count) -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class ChromePage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"chrome-shown", false};
  static constexpr std::string_view button = "navigation-chrome";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
