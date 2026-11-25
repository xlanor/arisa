#pragma once

#include <arisa/composition/anchor.hpp>
#include <arisa/composition/app_storage.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/preference_key.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/scene_storage.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace arisa::examples {

class DataFlowPage final : public composition::Component {
 public:
  class Opener;
  class Inbox;
  class Tile;
  class Traits;

  using Selection = std::optional<composition::Anchor<composition::Rect>>;

  static constexpr int tile_count = 4;
  static constexpr float tile_width = 120.0F;
  static constexpr float tile_height = 64.0F;
  static constexpr float stack_height = 300.0F;
  static constexpr std::string_view close_button = "data-flow-close";
  static constexpr std::string_view tiles_view = "data-flow-tiles";
  static constexpr std::string_view marker_view = "data-flow-marker";
  static constexpr std::string_view hints_toggle = "data-flow-hints";
  static constexpr std::string_view tab_picker = "data-flow-tab";
  static constexpr std::string_view link_view = "data-flow-link";
  static constexpr composition::AppStorage<bool> hints_key{"gallery.hints",
                                                           false};
  static constexpr composition::SceneStorage<int> tab_key{"gallery.tab", 0};
  static const composition::PreferenceKey<Selection> selection;

  explicit DataFlowPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const DataFlowPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto tile_key(int index) -> std::string;

 private:
  static auto section(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto titles() -> composition::Element;

  static auto tiles() -> composition::Element;

  static auto marker(const Selection& chosen) -> composition::Element;

  static auto stored(composition::Context& context) -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class DataFlowPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"data-flow-shown",
                                                         false};
  static constexpr std::string_view button = "navigation-data-flow";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

class DataFlowPage::Inbox final : public composition::Component {
 public:
  static constexpr composition::StateKey<int> unread_key{"data-flow-unread", 3};
  static constexpr std::string_view unread_button = "data-flow-unread";

  auto operator==(const Inbox&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

class DataFlowPage::Tile final : public composition::Component {
 public:
  explicit Tile(int index) noexcept : index_(index) {}

  auto operator==(const Tile&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  int index_;
};

class DataFlowPage::Traits final : public composition::Component {
 public:
  auto operator==(const Traits&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto row(std::string_view name, std::string value)
      -> composition::Element;
};

}
