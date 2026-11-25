#include <arisa/components/vstack.hpp>
#include <arisa/composition/command.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/enablement.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/composition/tree.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace arisa::composition {

class ControlSupportTest : public ::testing::Test {
 protected:
  struct Seen {
    std::vector<bool> enabled;
    std::vector<bool> focused;
    std::vector<bool> pressed;
    std::vector<float> widths;
  };

  class Reader final : public Component {
   public:
    explicit Reader(Seen* seen) noexcept : seen_(seen) {}

    auto operator==(const Reader&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      seen_->enabled.push_back(context.environment(Enablement::is_enabled));
      seen_->focused.push_back(context.is_focused());
      seen_->pressed.push_back(context.is_pressed());
      return components::VStack();
    }

   private:
    Seen* seen_;
  };

  static auto owner() -> Tree::OwnerCheck {
    return [] { return true; };
  }

  static auto find_reader(const Node& node) -> std::optional<NodeId> {
    if (node.component_as<Reader>().has_value()) {
      return node.id();
    }
    for (const Node& child : node.children()) {
      if (const auto found = find_reader(child)) {
        return found;
      }
    }
    return std::nullopt;
  }
};

TEST_F(ControlSupportTest, TagValuesCompareByTypeAndValue) {
  const auto three = TagValue::of(3);
  const auto also_three = TagValue::of(3);
  const auto four = TagValue::of(4);
  const auto word = TagValue::of(std::string{"3"});
  EXPECT_TRUE(three->equals(*also_three));
  EXPECT_FALSE(three->equals(*four));
  EXPECT_FALSE(three->equals(*word));
  ASSERT_NE(three->get<int>(), nullptr);
  EXPECT_EQ(*three->get<int>(), 3);
  EXPECT_EQ(three->get<std::string>(), nullptr);
}

TEST_F(ControlSupportTest, ElementsCarryTheirTag) {
  Element tagged = components::VStack().tag(7);
  ASSERT_NE(tagged.tag(), nullptr);
  EXPECT_EQ(*tagged.tag()->get<int>(), 7);
  const Element plain = components::VStack();
  EXPECT_EQ(plain.tag(), nullptr);
}

TEST_F(ControlSupportTest, AxisFilteredMoveCommandsOnlyHandleTheirAxis) {
  const Command sideways =
      Command::move([](MoveCommandDirection) {}, Axis::horizontal);
  EXPECT_TRUE(sideways.handles(MoveCommandDirection::left));
  EXPECT_TRUE(sideways.handles(MoveCommandDirection::right));
  EXPECT_FALSE(sideways.handles(MoveCommandDirection::up));
  EXPECT_FALSE(sideways.handles(MoveCommandDirection::down));
  const Command any = Command::move([](MoveCommandDirection) {});
  EXPECT_TRUE(any.handles(MoveCommandDirection::up));
  const Command exit = Command::exit("Back", [] {});
  EXPECT_FALSE(exit.handles(MoveCommandDirection::left));
}

TEST_F(ControlSupportTest, DisabledSubtreesReadAsNotEnabled) {
  Seen seen;
  Tree tree{components::VStack(Reader{&seen}).disabled(true), owner()};
  tree.reconcile();
  ASSERT_FALSE(seen.enabled.empty());
  EXPECT_FALSE(seen.enabled.back());
  Seen open;
  Tree enabled{components::VStack(Reader{&open}).disabled(false), owner()};
  enabled.reconcile();
  ASSERT_FALSE(open.enabled.empty());
  EXPECT_TRUE(open.enabled.back());
}

TEST_F(ControlSupportTest, HighlightChangesReevaluateOnlyReaders) {
  Seen seen;
  Tree tree{components::VStack(Reader{&seen}), owner()};
  tree.reconcile();
  const auto reader = find_reader(tree.root());
  ASSERT_TRUE(reader.has_value());
  ASSERT_EQ(seen.focused.size(), 1U);
  EXPECT_FALSE(seen.focused.back());
  tree.highlight(*reader, std::nullopt);
  EXPECT_TRUE(tree.dirty());
  tree.reconcile();
  ASSERT_EQ(seen.focused.size(), 2U);
  EXPECT_TRUE(seen.focused.back());
  EXPECT_FALSE(seen.pressed.back());
  tree.highlight(*reader, *reader);
  tree.reconcile();
  EXPECT_TRUE(seen.pressed.back());
  tree.highlight(*reader, *reader);
  EXPECT_FALSE(tree.dirty());
}

TEST_F(ControlSupportTest, GeometryObserversFireWhenTheFrameChanges) {
  auto widths = std::make_shared<std::vector<float>>();
  Tree tree{components::VStack().on_geometry_change(
                std::type_identity<float>{},
                [](const GeometryProxy& proxy) { return proxy.size().width; },
                [widths](const float& width) { widths->push_back(width); }),
            owner()};
  tree.reconcile();
  const NodeId child = (*tree.root().children().begin()).id();
  tree.assign_frame(
      child, Rect{.x = 0.0F, .y = 0.0F, .width = 120.0F, .height = 10.0F});
  tree.assign_frame(
      child, Rect{.x = 5.0F, .y = 0.0F, .width = 120.0F, .height = 10.0F});
  tree.assign_frame(
      child, Rect{.x = 0.0F, .y = 0.0F, .width = 80.0F, .height = 10.0F});
  EXPECT_EQ(*widths, (std::vector<float>{120.0F, 80.0F}));
}

}
