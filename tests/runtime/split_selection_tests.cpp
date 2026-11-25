#include <arisa/components/button.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/section.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using provider::Button;

class SplitSelectionTest : public FocusFixture {
 protected:
  static constexpr int row_count = 8;

  struct Record {
    std::vector<int> built;
    std::map<int, composition::StateHandle<int>> counts;
    std::optional<composition::StateHandle<int>> selection;
    std::optional<composition::StateHandle<bool>> mode;
  };

  class Page final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> count{"split-page-count", 0};

    Page(int row, bool mode, bool actions, Record* record) noexcept
        : row_(row), mode_(mode), actions_(actions), record_(record) {}

    auto operator==(const Page&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      record_->built.push_back(row_);
      const auto held = context.state(count);
      record_->counts.insert_or_assign(row_, held);
      Element label = components::Text(std::format(
          "detail {} count {}{}", row_, held.get(), mode_ ? " on" : ""));
      if (!actions_) {
        return label;
      }
      return components::VStack(std::move(label),
                                components::Button("Act", [] {}).id("act"));
    }

   private:
    int row_;
    bool mode_;
    bool actions_;
    Record* record_;
  };

  class Screen final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> selected{"split-selected", 0};
    static constexpr composition::StateKey<bool> flag{"split-mode", false};

    Screen(
        Record* record, bool actions,
        std::optional<std::chrono::milliseconds> delay = std::nullopt) noexcept
        : record_(record), actions_(actions), delay_(delay) {}

    auto operator==(const Screen&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto selection = context.state(selected);
      const auto mode = context.state(flag);
      record_->selection = selection;
      record_->mode = mode;
      std::vector<Element> rows;
      for (int row = 0; row < row_count; ++row) {
        rows.push_back(components::Text(std::format("Row {}", row)).tag(row));
      }
      Element split =
          Element{
              structure::NavigationSplitView(
                  structure::List(selection, structure::Section(
                                                 structure::Section::Labels{},
                                                 std::move(rows))),
                  Page{selection.get(), mode.get(), actions_, record_})}
              .list_style(structure::ListStyle::sidebar());
      if (delay_.has_value()) {
        split = std::move(split).detail_settle_delay(*delay_);
      }
      return split;
    }

   private:
    Record* record_;
    bool actions_;
    std::optional<std::chrono::milliseconds> delay_;
  };

  void step(Application& application, Button direction) {
    providers_.input().push(pad({direction}));
    providers_.input().push(pad({}));
    frames(application, 2);
    ASSERT_TRUE(providers_.clock().advance(frame_step).has_value());
    frames(application, 1);
  }

  void settle(Application& application) {
    for (int index = 0; index < 40; ++index) {
      ASSERT_TRUE(providers_.clock().advance(frame_step).has_value());
      frames(application, 1);
    }
  }

  static auto row_key(int row) -> std::string {
    return std::format("arisa.list.row.{}", row);
  }

  static auto page_opacity(Application& application, std::int64_t id)
      -> std::optional<float> {
    const auto& tree = application.root().tree();
    const auto found = tree.find(composition::ElementKey{
        std::format("{}.{}", structure::NavigationSplitView::page_space, id)});
    if (!found.has_value()) {
      return std::nullopt;
    }
    return tree.node(*found)->get().modifiers().opacity;
  }

  provider::headless::HeadlessProviderFactory providers_;
};

TEST_F(SplitSelectionTest, AFastFocusWalkBuildsTheDetailOnceAtTheEnd) {
  Record record;
  Application application = start(providers_, Screen{&record, false});
  settle(application);
  ASSERT_TRUE(shows(application, "detail 0 count 0"));
  record.built.clear();
  for (int row = 1; row < row_count; ++row) {
    step(application, Button::dpad_down);
    EXPECT_EQ(record.selection->get(), row);
    ASSERT_TRUE(providers_.clock().advance(frame_step * 3).has_value());
    frames(application, 1);
  }
  EXPECT_TRUE(record.built.empty());
  EXPECT_TRUE(shows(application, "detail 0 count 0"));
  settle(application);
  EXPECT_TRUE(
      shows(application, std::format("detail {} count 0", row_count - 1)));
  EXPECT_EQ(record.built, std::vector<int>{row_count - 1});
}

TEST_F(SplitSelectionTest, ATapSwitchesTheDetailImmediately) {
  Record record;
  Application application = start(providers_, Screen{&record, false});
  settle(application);
  record.built.clear();
  tap(providers_, application, centre(application, row_key(3)));
  EXPECT_EQ(record.selection->get(), 3);
  EXPECT_TRUE(shows(application, "detail 3 count 0"));
  EXPECT_EQ(record.built, std::vector<int>{3});
}

TEST_F(SplitSelectionTest, AProgrammaticSelectionSwitchesTheDetailImmediately) {
  Record record;
  Application application = start(providers_, Screen{&record, true});
  settle(application);
  press(providers_, application, Button::dpad_right);
  ASSERT_EQ(focused_key(application), "act");
  record.built.clear();
  record.selection->set(5);
  frames(application, 2);
  EXPECT_TRUE(shows(application, "detail 5 count 0"));
  EXPECT_EQ(record.built, std::vector<int>{5});
}

TEST_F(SplitSelectionTest, CachedPagesKeepTheirStateAndFreezeWhileHidden) {
  Record record;
  Application application = start(providers_, Screen{&record, false});
  settle(application);
  record.counts.at(0).set(5);
  settle(application);
  ASSERT_TRUE(shows(application, "detail 0 count 5"));
  tap(providers_, application, centre(application, row_key(1)));
  settle(application);
  ASSERT_TRUE(shows(application, "detail 1 count 0"));
  EXPECT_EQ(page_opacity(application, 0), 0.0F);
  record.built.clear();
  record.mode->set(true);
  settle(application);
  EXPECT_EQ(record.built, std::vector<int>{1});
  EXPECT_TRUE(shows(application, "detail 1 count 0 on"));
  EXPECT_TRUE(shows(application, "detail 0 count 5"));
  tap(providers_, application, centre(application, row_key(0)));
  settle(application);
  EXPECT_TRUE(shows(application, "detail 0 count 5 on"));
  EXPECT_EQ(record.counts.at(0).get(), 5);
}

TEST_F(SplitSelectionTest, AZeroSettleDelayFollowsEveryStep) {
  Record record;
  Application application = start(
      providers_, Screen{&record, false, std::chrono::milliseconds::zero()});
  settle(application);
  record.built.clear();
  for (int row = 1; row < 4; ++row) {
    step(application, Button::dpad_down);
    EXPECT_TRUE(shows(application, std::format("detail {} count 0", row)));
  }
  EXPECT_EQ(record.built, (std::vector<int>{1, 2, 3}));
}

}
