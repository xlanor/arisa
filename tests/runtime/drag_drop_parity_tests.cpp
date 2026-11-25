#include <arisa/components/button.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/drop_delegate.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/pasteboard.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/controls/paste_button.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/memory_clipboard.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

class Rows {
 public:
  static auto button(std::string key, float width = 100.0F,
                     float height = 50.0F) -> composition::Element {
    return components::Button(key, [] {})
        .frame({.width = width, .height = height})
        .id(std::move(key));
  }
};

using namespace std::chrono_literals;
using composition::Element;

class DragDropParityTest : public ScrollFixture {
 public:
  struct Log {
    std::vector<std::string> events;
    std::vector<composition::DragSession::Phase> drags;
    std::vector<composition::DropSession::Phase> drops;
    std::vector<std::string> items;
    std::size_t dragged_index{0};
    std::vector<int> dragged_ids;
  };

  struct Card {
    int id{0};
    std::string name;

    auto encoded() const -> std::string { return name; }

    static auto decoded(std::string_view text) -> std::optional<Card> {
      return Card{.id = 0, .name = std::string{text}};
    }

    static auto content_type() -> std::string { return "com.arisa.card"; }

    friend auto operator==(const Card&, const Card&) -> bool = default;
  };

  static auto slot(std::string key) -> Element {
    return components::ZStack()
        .frame({.width = 300.0F, .height = 100.0F})
        .focusable()
        .id(std::move(key));
  }

  static void carry(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, provider::PixelPoint from,
                    provider::PixelPoint to) {
    touch_down(providers, application, from);
    step(providers, application, 550ms);
    frames(application, 2);
    for (int index = 1; index <= 6; ++index) {
      const float progress = static_cast<float>(index) / 6.0F;
      touch_move(
          providers, application,
          provider::PixelPoint{.x = from.x + ((to.x - from.x) * progress),
                               .y = from.y + ((to.y - from.y) * progress)});
    }
    frames(application, 2);
    touch_up(providers, application, to);
    frames(application, 3);
  }

  class Delegate final : public composition::DropDelegate {
   public:
    explicit Delegate(std::shared_ptr<Log> log) : log_(std::move(log)) {}

    auto validate_drop(const composition::DropInfo& info) -> bool override {
      const std::array types{composition::UTType::plain_text()};
      return info.has_items_conforming_to(types);
    }

    auto perform_drop(const composition::DropInfo& info) -> bool override {
      const std::array types{composition::UTType::plain_text()};
      for (const composition::TransferItem& item : info.item_providers(types)) {
        log_->items.push_back(item.data());
      }
      log_->events.emplace_back("perform");
      return true;
    }

    void drop_entered(const composition::DropInfo&) override {
      log_->events.emplace_back("entered");
    }

    auto drop_updated(const composition::DropInfo&)
        -> std::optional<composition::DropProposal> override {
      return composition::DropProposal{composition::DropOperation::copy};
    }

    void drop_exited(const composition::DropInfo&) override {
      log_->events.emplace_back("exited");
    }

   private:
    std::shared_ptr<Log> log_;
  };
};

TEST_F(DragDropParityTest, SessionsReportPhasesAtSourceAndTarget) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(
          button("source")
              .draggable(std::string{"tile"})
              .on_drag_session_updated(
                  [log](const composition::DragSession& s) {
                    log->drags.push_back(s.phase());
                  }),
          slot("slot")
              .drop_destination(std::type_identity<std::string>{}, true,
                                [log](std::vector<std::string> items,
                                      const composition::DropSession& session) {
                                  log->items = std::move(items);
                                  log->events.push_back(
                                      std::format("{}", session.items_count()));
                                })
              .on_drop_session_updated(
                  [log](const composition::DropSession& s) {
                    log->drops.push_back(s.phase());
                  })
              .drop_configuration([](const composition::DropSession&) {
                return composition::DropConfiguration{
                    composition::DropOperation::copy};
              })));
  carry(providers, application, centre(application, "source"),
        centre(application, "slot"));
  EXPECT_EQ(log->items, (std::vector<std::string>{"tile"}));
  EXPECT_EQ(log->events, (std::vector<std::string>{"1"}));
  ASSERT_FALSE(log->drags.empty());
  EXPECT_EQ(log->drags.front(), composition::DragSession::Phase::initial());
  EXPECT_EQ(log->drags.back(),
            composition::DragSession::Phase::data_transfer_completed());
  ASSERT_FALSE(log->drops.empty());
  EXPECT_EQ(log->drops.front(), composition::DropSession::Phase::entering());
  EXPECT_TRUE(
      std::ranges::contains(log->drops, composition::DropSession::Phase::ended(
                                            composition::DropOperation::copy)));
}

TEST_F(DragDropParityTest, ConfigurationForbidsDisallowedOperations) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(
          button("source")
              .draggable(std::string{"tile"})
              .drag_configuration(composition::DragConfiguration{false})
              .on_drag_session_updated(
                  [log](const composition::DragSession& s) {
                    log->drags.push_back(s.phase());
                  }),
          slot("slot")
              .drop_destination(std::type_identity<std::string>{}, true,
                                [log](std::vector<std::string> items,
                                      const composition::DropSession&) {
                                  log->items = std::move(items);
                                })
              .drop_configuration([](const composition::DropSession&) {
                return composition::DropConfiguration{
                    composition::DropOperation::move};
              }),
          slot("closed").drop_destination(
              std::type_identity<std::string>{}, false,
              [log](std::vector<std::string>, const composition::DropSession&) {
                log->events.emplace_back("closed");
              })));
  carry(providers, application, centre(application, "source"),
        centre(application, "slot"));
  EXPECT_TRUE(log->items.empty());
  ASSERT_FALSE(log->drags.empty());
  EXPECT_EQ(log->drags.back(), composition::DragSession::Phase::ended(
                                   composition::DropOperation::cancel));
  carry(providers, application, centre(application, "source"),
        centre(application, "closed"));
  EXPECT_TRUE(log->events.empty());
}

TEST_F(DragDropParityTest, DelegatesAndItemProviderFormsReceiveDrops) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  static constexpr composition::StateKey<bool> over{"parity-over", false};
  class Targets final : public composition::Component {
   public:
    explicit Targets(std::shared_ptr<Log> log) : log_(std::move(log)) {}

    auto operator==(const Targets& other) const -> bool {
      return log_ == other.log_;
    }

    auto body(composition::Context& context) const -> Element override {
      const auto inside = context.state(over);
      std::vector<composition::UTType> text;
      text.push_back(composition::UTType::plain_text());
      return components::VStack(
          Rows::button("source").on_drag([] {
            return composition::TransferItem::of(std::string{"given"});
          }),
          slot("delegate").on_drop(text, std::make_shared<Delegate>(log_)),
          slot("provider")
              .on_drop(
                  text, composition::Binding<bool>{inside},
                  [log = log_](std::vector<composition::TransferItem> items,
                               composition::Point) {
                    log->items.push_back(items.front().data() + "!");
                    return true;
                  }),
          components::Text(inside.get() ? "over" : "away"));
    }

   private:
    std::shared_ptr<Log> log_;
  };
  Application application = start(providers, Targets{log});
  carry(providers, application, centre(application, "source"),
        centre(application, "delegate"));
  EXPECT_EQ(log->items, (std::vector<std::string>{"given"}));
  EXPECT_TRUE(std::ranges::contains(log->events, "entered"));
  EXPECT_TRUE(std::ranges::contains(log->events, "perform"));
  carry(providers, application, centre(application, "source"),
        centre(application, "provider"));
  EXPECT_EQ(log->items.back(), "given!");
  EXPECT_TRUE(shows(application, "away"));
}

class ContainerProbe final : public composition::Component {
 public:
  static constexpr composition::Namespace space{"parity-drag-container"};

  explicit ContainerProbe(std::shared_ptr<DragDropParityTest::Log> log)
      : log_(std::move(log)) {}

  auto operator==(const ContainerProbe& other) const -> bool {
    return log_ == other.log_;
  }

  auto body(composition::Context& context) const -> Element override {
    using Card = DragDropParityTest::Card;
    const composition::Namespace::ID scope = context.namespace_id(space);
    std::vector<Element> cards;
    for (int id = 1; id <= 3; ++id) {
      cards.push_back(Rows::button(std::format("card{}", id))
                          .draggable_container_item(id, scope));
    }
    return components::VStack(
        components::HStack(std::move(cards))
            .drag_container(std::type_identity<Card>{}, scope,
                            [](std::vector<int> ids) {
                              std::vector<Card> made;
                              for (const int id : ids) {
                                made.push_back(Card{
                                    .id = id, .name = std::format("c{}", id)});
                              }
                              return made;
                            })
            .drag_container_selection(std::vector<int>{1, 3}, scope)
            .drag_previews_formation(
                composition::DragDropPreviewsFormation::pile)
            .on_drag_session_updated(
                [log = log_](const composition::DragSession& session) {
                  log->dragged_index = session.dragged_item_index();
                  log->dragged_ids = session.dragged_item_ids<int>();
                }),
        Rows::button("solo").draggable(
            std::type_identity<Card>{},
            std::optional<Card>{Card{.id = 9, .name = "nine"}}),
        Rows::button("keyed").draggable(
            std::type_identity<Card>{}, &Card::id, std::nullopt,
            [] { return std::optional<Card>{Card{.id = 5, .name = "five"}}; }),
        components::ZStack()
            .frame({.width = 300.0F, .height = 100.0F})
            .drop_destination(
                std::type_identity<Card>{},
                [log = log_](std::vector<Card> items, composition::Point) {
                  log->items.clear();
                  for (const Card& card : items) {
                    log->items.push_back(card.name);
                  }
                  return true;
                })
            .drop_previews_formation(
                composition::DragDropPreviewsFormation::list)
            .id("tray"));
  }

 private:
  std::shared_ptr<DragDropParityTest::Log> log_;
};

TEST_F(DragDropParityTest, ContainersDragTheSelectionTogether) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(providers, ContainerProbe{log});
  const provider::PixelPoint from = centre(application, "card3");
  touch_down(providers, application, from);
  step(providers, application, 550ms);
  frames(application, 2);
  ASSERT_TRUE(application.interaction().drag().has_value());
  EXPECT_EQ(application.interaction().drag()->count, 2U);
  EXPECT_EQ(application.interaction().drag()->formation,
            composition::DragDropPreviewsFormation::pile);
  EXPECT_TRUE(shows(application, "2"));
  const provider::PixelPoint to = centre(application, "tray");
  for (int index = 1; index <= 6; ++index) {
    const float progress = static_cast<float>(index) / 6.0F;
    touch_move(
        providers, application,
        provider::PixelPoint{.x = from.x + ((to.x - from.x) * progress),
                             .y = from.y + ((to.y - from.y) * progress)});
  }
  frames(application, 2);
  EXPECT_EQ(application.interaction().drag()->formation,
            composition::DragDropPreviewsFormation::list);
  touch_up(providers, application, to);
  frames(application, 3);
  EXPECT_EQ(log->items, (std::vector<std::string>{"c1", "c3"}));
  EXPECT_EQ(log->dragged_index, 1U);
  EXPECT_EQ(log->dragged_ids, (std::vector<int>{1, 3}));
  carry(providers, application, centre(application, "solo"), to);
  EXPECT_EQ(log->items, (std::vector<std::string>{"nine"}));
  carry(providers, application, centre(application, "keyed"), to);
  EXPECT_EQ(log->items, (std::vector<std::string>{"five"}));
}

TEST_F(DragDropParityTest, HoveringADragSpringLoadsAButton) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, components::VStack(
                     button("source").draggable(std::string{"tile"}),
                     components::Button(
                         "open", [log] { log->events.emplace_back("open"); })
                         .frame({.width = 200.0F, .height = 80.0F})
                         .spring_loading_behavior(
                             composition::SpringLoadingBehavior::enabled)
                         .id("open"),
                     components::Button(
                         "shut", [log] { log->events.emplace_back("shut"); })
                         .frame({.width = 200.0F, .height = 80.0F})
                         .spring_loading_behavior(
                             composition::SpringLoadingBehavior::disabled)
                         .id("shut")));
  const provider::PixelPoint from = centre(application, "source");
  touch_down(providers, application, from);
  step(providers, application, 550ms);
  frames(application, 2);
  touch_move(providers, application, centre(application, "shut"));
  for (int index = 0; index < 60; ++index) {
    step(providers, application, 16ms);
  }
  EXPECT_TRUE(log->events.empty());
  touch_move(providers, application, centre(application, "open"));
  for (int index = 0; index < 60; ++index) {
    step(providers, application, 16ms);
  }
  EXPECT_EQ(log->events, (std::vector<std::string>{"open"}));
  touch_up(providers, application, centre(application, "open"));
  frames(application, 2);
}

struct Row {
  int id{0};
  std::string name;

  friend auto operator==(const Row&, const Row&) -> bool = default;
};

class ReorderProbe final : public composition::Component {
 public:
  static constexpr composition::StateKey<std::vector<Row>> left{"parity-left",
                                                                {}};
  static constexpr composition::StateKey<std::vector<Row>> right{"parity-right",
                                                                 {}};

  auto operator==(const ReorderProbe&) const -> bool = default;

  auto body(composition::Context& context) const -> Element override {
    const auto lefts =
        context.state(left, std::vector<Row>{Row{.id = 1, .name = "a"},
                                             Row{.id = 2, .name = "b"},
                                             Row{.id = 3, .name = "c"}});
    const auto rights =
        context.state(right, std::vector<Row>{Row{.id = 4, .name = "d"}});
    const auto row = [](const Row& each) {
      return Rows::button(each.name, 120.0F, 50.0F);
    };
    std::string order;
    for (const Row& each : lefts.get()) {
      order += each.name;
    }
    order += "|";
    for (const Row& each : rights.get()) {
      order += each.name;
    }
    return components::VStack(
        components::HStack(
            components::VStack(
                components::ForEach(lefts.get(), &Row::id, row)
                    .reorderable(composition::ElementKey{"left"})),
            components::VStack(
                components::ForEach(rights.get(), &Row::id, row)
                    .reorderable(composition::ElementKey{"right"})))
            .reorder_container(
                std::type_identity<Row>{}, std::type_identity<std::string>{},
                true,
                [lefts, rights](
                    composition::ReorderDifference<int, std::string> moved) {
                  std::vector<Row> all = lefts.get();
                  all.insert(all.end(), rights.get().begin(),
                             rights.get().end());
                  std::vector<Row> taken;
                  std::erase_if(all, [&](const Row& each) {
                    if (std::ranges::contains(moved.sources(), each.id)) {
                      taken.push_back(each);
                      return true;
                    }
                    return false;
                  });
                  std::vector<Row> into =
                      moved.destination().collection_id() == "left"
                          ? lefts.get()
                          : rights.get();
                  std::erase_if(into, [&](const Row& each) {
                    return std::ranges::contains(moved.sources(), each.id);
                  });
                  auto at = into.end();
                  if (const auto& before =
                          moved.destination().position().item()) {
                    at = std::ranges::find(into, *before, &Row::id);
                  }
                  into.insert(at, taken.begin(), taken.end());
                  std::vector<Row> other =
                      moved.destination().collection_id() == "left"
                          ? rights.get()
                          : lefts.get();
                  std::erase_if(other, [&](const Row& each) {
                    return std::ranges::contains(moved.sources(), each.id);
                  });
                  if (moved.destination().collection_id() == "left") {
                    lefts.set(into);
                    rights.set(other);
                  } else {
                    rights.set(into);
                    lefts.set(other);
                  }
                }),
        components::Text(order));
  }
};

TEST_F(DragDropParityTest, ReorderContainersMoveWithinAndAcrossCollections) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, ReorderProbe{});
  EXPECT_TRUE(shows(application, "abc|d"));
  const provider::PixelPoint c = centre(application, "c");
  const provider::PixelPoint a = centre(application, "a");
  carry(providers, application, c,
        provider::PixelPoint{.x = a.x, .y = a.y - 10.0F});
  frames(application, 2);
  EXPECT_TRUE(shows(application, "cab|d"));
  const provider::PixelPoint d = centre(application, "d");
  carry(providers, application, centre(application, "a"),
        provider::PixelPoint{.x = d.x, .y = d.y + 20.0F});
  frames(application, 2);
  EXPECT_TRUE(shows(application, "cb|da"));
  press(providers, application, provider::Button::dpad_left);
  press(providers, application, provider::Button::dpad_left);
  press(providers, application, provider::Button::dpad_up);
  press(providers, application, provider::Button::dpad_up);
  EXPECT_EQ(focused_key(application), "3");
  press(providers, application, provider::Button::x);
  ASSERT_TRUE(application.interaction().drag().has_value());
  press(providers, application, provider::Button::dpad_down);
  press(providers, application, provider::Button::a);
  frames(application, 3);
  EXPECT_TRUE(shows(application, "bc|da"));
}

TEST_F(DragDropParityTest, PasteFormsAndItemProvidersUseTheClipboard) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  ASSERT_TRUE(providers.clipboard().write_text("pasted").has_value());
  std::vector<std::string> identifiers{"public.plain-text"};
  Application application =
      start(providers,
            components::VStack(
                controls::PasteButton(
                    identifiers,
                    [log](std::vector<composition::TransferItem> items) {
                      log->items.push_back(items.front().data());
                    })
                    .id("paste"),
                controls::PasteButton(
                    std::vector<composition::UTType>{
                        composition::UTType::plain_text()},
                    [](std::vector<composition::TransferItem> items)
                        -> std::optional<std::string> {
                      return items.empty()
                                 ? std::nullopt
                                 : std::optional{items.front().data() + "?"};
                    },
                    [log](std::string text) {
                      log->items.push_back(std::move(text));
                    })
                    .id("validated"),
                Rows::button("command").on_paste_command(
                    std::vector<composition::UTType>{
                        composition::UTType::plain_text()},
                    [](std::vector<composition::TransferItem> items)
                        -> std::optional<std::size_t> { return items.size(); },
                    [log](std::size_t count) {
                      log->events.push_back(std::to_string(count));
                    })));
  tap(providers, application, centre(application, "paste"));
  tap(providers, application, centre(application, "validated"));
  EXPECT_EQ(log->items, (std::vector<std::string>{"pasted", "pasted?"}));
}

TEST_F(DragDropParityTest, NonTextItemsCrossTheSystemClipboard) {
  provider::headless::MemoryClipboard clipboard;
  const composition::Pasteboard board{clipboard};
  board.set_items(
      {composition::TransferItem{composition::UTType{"public.png"}, "pixels"},
       composition::TransferItem::of(std::string{"label"})});
  EXPECT_EQ(clipboard.mime_types().size(), 2U);
  EXPECT_EQ(clipboard.read_entry("image/png"), "pixels");
  const composition::Pasteboard reader{clipboard};
  const std::vector<composition::TransferItem> read = reader.items();
  ASSERT_EQ(read.size(), 2U);
  EXPECT_TRUE(reader.contains(composition::UTType{"public.png"}));
  ASSERT_TRUE(clipboard.write_text("outside").has_value());
  EXPECT_EQ(board.items().front().data(), "outside");
}

}
