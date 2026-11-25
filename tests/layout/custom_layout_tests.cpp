#include "layout_fixture.hpp"

#include <arisa/components/any_layout.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/hstack_layout.hpp>
#include <arisa/components/layout_subviews.hpp>
#include <arisa/components/subviews_collection.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/view_spacing.hpp>
#include <arisa/components/view_that_fits.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/vstack_layout.hpp>
#include <arisa/components/zstack_layout.hpp>
#include <arisa/composition/alignment_id.hpp>
#include <arisa/composition/alignment_key.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/container_value_key.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/layout_value_key.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/view_dimensions.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/theme.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::layout {

using components::HorizontalAlignment;
using components::LayoutSubviews;
using components::ProposedViewSize;
using components::VerticalAlignment;
using composition::ViewDimensions;

class CustomLayoutTest : public ::testing::Test, protected LayoutFixture {
 protected:
  static constexpr float tolerance = 0.01F;

  class Flow {
   public:
    explicit Flow(float spacing) noexcept : spacing_(spacing) {}

    auto size_that_fits(ProposedViewSize proposal,
                        const LayoutSubviews& subviews) const
        -> composition::Size {
      const float limit = proposal.width.value_or(composition::infinity);
      float x = 0.0F;
      float y = 0.0F;
      float line = 0.0F;
      float widest = 0.0F;
      for (const components::LayoutSubview& subview : subviews) {
        const composition::Size size =
            subview.size_that_fits(ProposedViewSize::unspecified);
        if (x > 0.0F && x + size.width > limit) {
          y += line + spacing_;
          x = 0.0F;
          line = 0.0F;
        }
        x += size.width + spacing_;
        widest = std::max(widest, x - spacing_);
        line = std::max(line, size.height);
      }
      return composition::Size{.width = proposal.width.value_or(widest),
                               .height = y + line};
    }

    void place_subviews(composition::Rect bounds, ProposedViewSize,
                        const LayoutSubviews& subviews) const {
      float x = bounds.x;
      float y = bounds.y;
      float line = 0.0F;
      for (const components::LayoutSubview& subview : subviews) {
        const composition::Size size =
            subview.size_that_fits(ProposedViewSize::unspecified);
        if (x > bounds.x && x + size.width > bounds.x + bounds.width) {
          y += line + spacing_;
          x = bounds.x;
          line = 0.0F;
        }
        subview.place(composition::Point{.x = x, .y = y});
        x += size.width + spacing_;
        line = std::max(line, size.height);
      }
    }

    friend auto operator==(const Flow&, const Flow&) -> bool = default;

   private:
    float spacing_;
  };

  class Spaced {
   public:
    auto size_that_fits(ProposedViewSize, const LayoutSubviews& subviews) const
        -> composition::Size {
      composition::Size total{};
      for (std::size_t index = 0; index < subviews.size(); ++index) {
        const composition::Size size =
            subviews[index].size_that_fits(ProposedViewSize::unspecified);
        total.width += size.width + gap(subviews, index);
        total.height = std::max(total.height, size.height);
      }
      return total;
    }

    void place_subviews(composition::Rect bounds, ProposedViewSize,
                        const LayoutSubviews& subviews) const {
      float x = bounds.x;
      for (std::size_t index = 0; index < subviews.size(); ++index) {
        x += gap(subviews, index);
        subviews[index].place(composition::Point{.x = x, .y = bounds.y});
        x +=
            subviews[index].size_that_fits(ProposedViewSize::unspecified).width;
      }
    }

    friend auto operator==(const Spaced&, const Spaced&) -> bool = default;

   private:
    static auto gap(const LayoutSubviews& subviews, std::size_t index)
        -> float {
      if (index == 0) {
        return 0.0F;
      }
      return subviews[index - 1].spacing().distance(
          subviews[index].spacing(), composition::Axis::horizontal);
    }
  };

  static constexpr composition::LayoutValueKey<int> rank{"rank", 0};

  class Ranked {
   public:
    using Cache = std::vector<std::size_t>;

    auto make_cache(const LayoutSubviews& subviews) const -> Cache {
      Cache order(subviews.size());
      for (std::size_t index = 0; index < order.size(); ++index) {
        order[index] = index;
      }
      std::ranges::stable_sort(order, {}, [&subviews](std::size_t index) {
        return subviews[index][rank];
      });
      return order;
    }

    auto size_that_fits(ProposedViewSize, const LayoutSubviews& subviews,
                        Cache&) const -> composition::Size {
      composition::Size total{};
      for (const components::LayoutSubview& subview : subviews) {
        const composition::Size size =
            subview.size_that_fits(ProposedViewSize::unspecified);
        total.width += size.width;
        total.height = std::max(total.height, size.height);
      }
      return total;
    }

    void place_subviews(composition::Rect bounds, ProposedViewSize,
                        const LayoutSubviews& subviews, Cache& order) const {
      float x = bounds.x;
      for (const std::size_t index : order) {
        subviews[index].place(composition::Point{.x = x, .y = bounds.y});
        x +=
            subviews[index].size_that_fits(ProposedViewSize::unspecified).width;
      }
    }

    friend auto operator==(const Ranked&, const Ranked&) -> bool = default;
  };

  static constexpr composition::AlignmentID colon{
      "colon", composition::Axis::horizontal,
      [](const ViewDimensions& dimensions) {
        return dimensions[HorizontalAlignment::leading];
      }};

  struct Laid {
    std::unique_ptr<composition::Tree> tree;
    LayoutContext context;
  };

  static auto laid(composition::Element content) -> Laid {
    auto built = tree(std::move(content));
    auto context = LayoutContext::create();
    EXPECT_TRUE(context.has_value());
    EXPECT_TRUE(
        context->update(*built, handheld, std::ref(typesetter())).has_value());
    for (int round = 0; round < 2 && built->dirty(); ++round) {
      built->reconcile();
      EXPECT_TRUE(context->update(*built, handheld, std::ref(typesetter()))
                      .has_value());
    }
    return Laid{.tree = std::move(built), .context = std::move(*context)};
  }

  static auto offset(const composition::Tree& built, std::string_view inner,
                     std::string_view outer) -> composition::Rect {
    const composition::Rect in = frame(built, inner);
    const composition::Rect out = frame(built, outer);
    return composition::Rect{.x = in.x - out.x,
                             .y = in.y - out.y,
                             .width = in.width,
                             .height = in.height};
  }

  class Switching final : public composition::Component {
   public:
    explicit Switching(std::shared_ptr<composition::Observable<int>> mode)
        : mode_(std::move(mode)) {}

    auto operator==(const Switching&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const int mode = context.read(*mode_);
      const components::AnyLayout layout =
          mode == 0   ? components::AnyLayout(components::HStackLayout{})
          : mode == 1 ? components::AnyLayout(components::VStackLayout{})
          : mode == 2 ? components::AnyLayout(Flow{8.0F})
                      : components::AnyLayout(components::ZStackLayout{});
      return layout(components::Text("first").id("first"),
                    components::Text("second").id("second"))
          .id("switched");
    }

   private:
    std::shared_ptr<composition::Observable<int>> mode_;
  };

  class Recoloured final : public composition::Component {
   public:
    explicit Recoloured(std::shared_ptr<composition::Observable<int>> step)
        : step_(std::move(step)) {}

    auto operator==(const Recoloured&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const int step = context.read(*step_);
      const theme::Theme base = theme::Theme::defaults();
      return components::HStack(
                 components::HStackOptions{.spacing = theme::Space{0.0F}},
                 box("lead", step == 0 ? 40.0F : 80.0F, 20.0F),
                 components::AnyLayout(components::HStackLayout(
                     components::HStackOptions{.spacing = theme::space::lg}))(
                     box("a", 30.0F, 20.0F), box("b", 30.0F, 20.0F))
                     .frame({.max_width = composition::infinity})
                     .environment(
                         components::Environment::theme,
                         step == 0
                             ? base
                             : base.with_accent(theme::Colour::hex(0xE6331AU)))
                     .id("row"))
          .frame({.width = 400.0F});
    }

   private:
    std::shared_ptr<composition::Observable<int>> step_;
  };

  static auto present(const composition::Tree& built, std::string_view key)
      -> bool {
    return built.find(composition::ElementKey{key}).has_value();
  }
};

TEST_F(CustomLayoutTest, AFlowLayoutWrapsTagsLikeSwiftUI) {
  const Laid result = laid(components::AnyLayout(Flow{8.0F})(
                               box("t0", 60.0F, 20.0F), box("t1", 80.0F, 20.0F),
                               box("t2", 50.0F, 20.0F), box("t3", 70.0F, 20.0F))
                               .frame({.width = 200.0F})
                               .id("flow"));
  const std::array expected{std::array{0.0F, 0.0F}, std::array{68.0F, 0.0F},
                            std::array{0.0F, 28.0F}, std::array{58.0F, 28.0F}};
  for (std::size_t index = 0; index < expected.size(); ++index) {
    const std::string key = std::format("t{}", index);
    const composition::Rect placed = offset(*result.tree, key, "flow");
    EXPECT_NEAR(placed.x, expected.at(index)[0], tolerance) << key;
    EXPECT_NEAR(placed.y, expected.at(index)[1], tolerance) << key;
  }
  EXPECT_NEAR(frame(*result.tree, "flow").height, 48.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "flow").width, 200.0F, tolerance);
}

TEST_F(CustomLayoutTest, AFlowLayoutTakesItsWidthInAStack) {
  const Laid result = laid(
      components::VStack(components::AnyLayout(Flow{8.0F})(
                             box("t0", 60.0F, 20.0F), box("t1", 80.0F, 20.0F),
                             box("t2", 50.0F, 20.0F))
                             .id("flow"))
          .frame({.width = 140.0F}));
  EXPECT_NEAR(frame(*result.tree, "flow").width, 140.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "t1", "flow").y, 28.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "t2", "flow").x, 88.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "t2", "flow").y, 28.0F, tolerance);
}

TEST_F(CustomLayoutTest, ViewSpacingGivesTheGapBetweenSubviews) {
  const Laid result = laid(components::AnyLayout(Spaced{})(
                               box("s0", 30.0F, 20.0F), box("s1", 30.0F, 20.0F),
                               box("s2", 30.0F, 20.0F))
                               .id("spaced"));
  const float first = offset(*result.tree, "s1", "spaced").x - 30.0F;
  const float second = offset(*result.tree, "s2", "spaced").x -
                       offset(*result.tree, "s1", "spaced").x - 30.0F;
  EXPECT_GT(first, 0.0F);
  EXPECT_NEAR(first, second, tolerance);
  const components::ViewSpacing plain;
  const components::ViewSpacing text{4.0F, 6.0F, true};
  EXPECT_FLOAT_EQ(plain.distance(components::ViewSpacing::zero,
                                 composition::Axis::horizontal),
                  0.0F);
  EXPECT_FLOAT_EQ(text.distance(text, composition::Axis::vertical), 6.0F);
  EXPECT_FLOAT_EQ(text.distance(plain, composition::Axis::vertical), 8.0F);
  components::ViewSpacing merged = text;
  merged.form_union(plain);
  EXPECT_FLOAT_EQ(merged.distance(text, composition::Axis::vertical), 8.0F);
}

TEST_F(CustomLayoutTest, LayoutValuesReachTheLayout) {
  const Laid result = laid(components::AnyLayout(Ranked{})(
                               box("a", 30.0F, 10.0F).layout_value(rank, 2),
                               box("b", 40.0F, 10.0F).layout_value(rank, 1))
                               .id("row"));
  EXPECT_NEAR(offset(*result.tree, "a", "row").x, 40.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "b", "row").x, 0.0F, tolerance);
}

TEST_F(CustomLayoutTest, ViewThatFitsPicksTheFirstVariantThatFits) {
  for (const auto& [width, wanted] :
       {std::pair{300.0F, std::string_view{"wide"}},
        std::pair{120.0F, std::string_view{"medium"}},
        std::pair{40.0F, std::string_view{"narrow"}}}) {
    const Laid result =
        laid(components::ViewThatFits(box("wide", 200.0F, 20.0F),
                                      box("medium", 100.0F, 20.0F),
                                      box("narrow", 60.0F, 20.0F))
                 .frame({.width = width})
                 .id("fits"));
    for (const std::string_view key : {"wide", "medium", "narrow"}) {
      const auto found = result.tree->find(composition::ElementKey{key});
      ASSERT_TRUE(found.has_value());
      const composition::Node& node = result.tree->node(*found)->get();
      EXPECT_EQ(result.tree->presentation(node).withheld(), key != wanted)
          << width << " " << key;
    }
    EXPECT_NEAR(frame(*result.tree, "fits").height, 20.0F, tolerance);
  }
}

TEST_F(CustomLayoutTest, ViewThatFitsCanFitOneAxis) {
  const Laid result = laid(components::ViewThatFits(composition::Axes::vertical,
                                                    box("tall", 400.0F, 80.0F),
                                                    box("short", 400.0F, 20.0F))
                               .frame({.width = 100.0F, .height = 40.0F}));
  const auto tall = result.tree->find(composition::ElementKey{"tall"});
  ASSERT_TRUE(tall.has_value());
  EXPECT_TRUE(
      result.tree->presentation(result.tree->node(*tall)->get()).withheld());
}

TEST_F(CustomLayoutTest, AnyLayoutMatchesTheStacks) {
  const Laid across = laid(components::AnyLayout(components::HStackLayout(
      components::HStackOptions{.spacing = theme::Space{4.0F}}))(
                               box("a", 30.0F, 20.0F), box("b", 40.0F, 10.0F))
                               .id("stack"));
  EXPECT_NEAR(offset(*across.tree, "b", "stack").x, 34.0F, tolerance);
  EXPECT_NEAR(offset(*across.tree, "b", "stack").y, 5.0F, tolerance);
  EXPECT_NEAR(frame(*across.tree, "stack").width, 74.0F, tolerance);
  EXPECT_NEAR(frame(*across.tree, "stack").height, 20.0F, tolerance);
  const Laid down = laid(components::AnyLayout(components::VStackLayout(
      components::VStackOptions{.spacing = theme::Space{4.0F}}))(
                             box("a", 30.0F, 20.0F), box("b", 40.0F, 10.0F))
                             .id("stack"));
  EXPECT_NEAR(offset(*down.tree, "b", "stack").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*down.tree, "b", "stack").y, 24.0F, tolerance);
  EXPECT_NEAR(frame(*down.tree, "stack").width, 40.0F, tolerance);
  EXPECT_NEAR(frame(*down.tree, "stack").height, 34.0F, tolerance);
}

TEST_F(CustomLayoutTest, SwitchingLayoutsKeepsIdentity) {
  auto mode = std::make_shared<composition::Observable<int>>(0);
  auto built =
      std::make_unique<composition::Tree>(Switching{mode}, [] { return true; });
  built->reconcile();
  const auto first = built->find(composition::ElementKey{"first"});
  const auto stack = built->find(composition::ElementKey{"switched"});
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(stack.has_value());
  for (const int next : {1, 2, 3, 0}) {
    mode->set(next);
    built->reconcile();
    EXPECT_EQ(built->find(composition::ElementKey{"first"}), first) << next;
    EXPECT_EQ(built->find(composition::ElementKey{"switched"}), stack) << next;
  }
}

TEST_F(CustomLayoutTest, AnAlignmentGuideMovesItsView) {
  const Laid result = laid(
      components::VStack(
          components::VStackOptions{.alignment = HorizontalAlignment::leading,
                                    .spacing = theme::Space{0.0F}},
          box("a", 40.0F, 10.0F)
              .alignment_guide(
                  HorizontalAlignment::leading,
                  [](const ViewDimensions& dimensions) {
                    return dimensions[HorizontalAlignment::trailing];
                  }),
          box("b", 60.0F, 10.0F))
          .id("stack"));
  EXPECT_NEAR(offset(*result.tree, "a", "stack").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "b", "stack").x, 40.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "stack").width, 100.0F, tolerance);
}

TEST_F(CustomLayoutTest, AConstantGuideShiftsTheOthers) {
  const Laid result = laid(
      components::VStack(
          components::VStackOptions{.alignment = HorizontalAlignment::leading,
                                    .spacing = theme::Space{0.0F}},
          box("a", 40.0F, 10.0F)
              .alignment_guide(HorizontalAlignment::leading,
                               [](const ViewDimensions&) { return 15.0F; }),
          box("b", 60.0F, 10.0F))
          .id("stack"));
  EXPECT_NEAR(offset(*result.tree, "a", "stack").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "b", "stack").x, 15.0F, tolerance);
}

TEST_F(CustomLayoutTest, ACustomAlignmentLinesUpFormFields) {
  const auto row = [](std::string_view label, std::string_view field,
                      float width) {
    return components::HStack(
        components::HStackOptions{.spacing = theme::Space{4.0F}},
        box(label, width, 10.0F),
        box(field, 50.0F, 10.0F)
            .alignment_guide(colon, [](const ViewDimensions& dimensions) {
              return dimensions[HorizontalAlignment::leading];
            }));
  };
  const Laid result =
      laid(components::VStack(
               components::VStackOptions{.spacing = theme::Space{0.0F},
                                         .guide = colon},
               row("l0", "f0", 30.0F), row("l1", "f1", 70.0F))
               .id("form"));
  EXPECT_NEAR(offset(*result.tree, "l0", "form").x, 40.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "f0", "form").x, 74.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "l1", "form").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "f1", "form").x, 74.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "form").width, 124.0F, tolerance);
}

TEST_F(CustomLayoutTest, AVerticalGuideMovesARowChild) {
  const Laid result = laid(
      components::HStack(
          components::HStackOptions{.alignment = VerticalAlignment::top,
                                    .spacing = theme::Space{0.0F}},
          box("a", 20.0F, 40.0F),
          box("b", 20.0F, 20.0F)
              .alignment_guide(VerticalAlignment::top,
                               [](const ViewDimensions& dimensions) {
                                 return dimensions[VerticalAlignment::bottom];
                               }))
          .id("row"));
  EXPECT_NEAR(offset(*result.tree, "a", "row").y, 20.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "b", "row").y, 0.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "row").height, 60.0F, tolerance);
}

TEST_F(CustomLayoutTest, LastTextBaselinesShareALine) {
  const Laid result =
      laid(components::HStack(
               components::HStackOptions{
                   .alignment = VerticalAlignment::last_text_baseline,
                   .spacing = theme::Space{0.0F}},
               components::Text("One\nTwo").id("two"),
               components::Text("Big").font(theme::Font::title).id("big"))
               .id("row"));
  const composition::Rect two = offset(*result.tree, "two", "row");
  const composition::Rect big = offset(*result.tree, "big", "row");
  EXPECT_NEAR(two.y, 0.0F, tolerance);
  EXPECT_GT(big.y, 0.0F);
  EXPECT_NEAR(two.y + two.height, big.y + big.height, 2.0F);
}

TEST_F(CustomLayoutTest, ASecondFrameWrapsTheFirst) {
  const Laid result =
      laid(components::VStack(
               components::VStackOptions{.spacing = theme::Space{0.0F}},
               box("inner", 30.0F, 10.0F)
                   .frame({.height = 40.0F})
                   .frame({.max_width = composition::infinity})
                   .id("outer"),
               box("rule", 200.0F, 2.0F))
               .frame({.width = 200.0F}));
  EXPECT_NEAR(frame(*result.tree, "outer").width, 200.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "outer").height, 40.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "inner", "outer").x, 85.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "inner", "outer").y, 15.0F, tolerance);
  const Laid widened = laid(components::VStack(
      box("framed", 30.0F, 10.0F)
          .frame({.width = 60.0F})
          .frame(
              {.width = 120.0F, .alignment = composition::Alignment::leading})
          .id("outer")));
  EXPECT_NEAR(frame(*widened.tree, "outer").width, 120.0F, tolerance);
}

TEST_F(CustomLayoutTest, SubviewsCarryContainerValuesAndIds) {
  static constexpr composition::ContainerValueKey<int> tint{"tint", 0};
  components::SubviewsCollection subviews{components::Group(
      components::Text("plain").id("plain"),
      components::ForEach(
          std::views::iota(0, 2), [](int index) { return index; },
          [](int index) {
            return components::Text(std::format("item {}", index))
                .container_value(tint, index + 1);
          }))};
  ASSERT_EQ(subviews.size(), 3U);
  EXPECT_EQ(subviews[0].id(), composition::ElementKey{"plain"});
  EXPECT_EQ(subviews[0][tint], 0);
  EXPECT_EQ(subviews[1].id(), composition::ElementKey{0});
  EXPECT_EQ(subviews[1][tint], 1);
  EXPECT_EQ(subviews[2][tint], 2);
}

TEST_F(CustomLayoutTest, ForEachSubviewWrapsEachSubview) {
  const Laid result = laid(components::VStack(components::ForEach(
      components::SubviewsCollection{
          components::Group(box("a", 20.0F, 10.0F), box("b", 30.0F, 10.0F))},
      [](components::Subview subview) {
        const std::string tail =
            std::format("{}-tail", subview.id().description());
        return components::HStack(
            components::HStackOptions{.spacing = theme::Space{0.0F}},
            std::move(subview).view(), box(tail, 5.0F, 10.0F));
      })));
  EXPECT_NEAR(frame(*result.tree, "a-tail").x,
              frame(*result.tree, "a").x + 20.0F, tolerance);
  EXPECT_NEAR(frame(*result.tree, "b-tail").x,
              frame(*result.tree, "b").x + 30.0F, tolerance);
}

TEST_F(CustomLayoutTest, GroupSubviewsSeesTheWholeCollection) {
  std::size_t counted = 0;
  const Laid result = laid(components::Group(
      components::SubviewsCollection{
          components::Group(box("a", 20.0F, 10.0F), box("b", 30.0F, 10.0F))},
      [&counted](components::SubviewsCollection subviews) {
        counted = subviews.size();
        std::vector<composition::Element> reversed;
        for (components::Subview& subview : subviews) {
          reversed.insert(reversed.begin(), std::move(subview).view());
        }
        return components::HStack(
            components::HStackOptions{.spacing = theme::Space{0.0F}},
            std::move(reversed));
      }));
  EXPECT_EQ(counted, 2U);
  EXPECT_LT(frame(*result.tree, "b").x, frame(*result.tree, "a").x);
}

}

#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/anchor.hpp>
#include <arisa/composition/anchor_source.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/paint_order.hpp>
#include <arisa/composition/preference_key.hpp>
#include <arisa/composition/scroll_presentation.hpp>

namespace arisa::layout {

TEST_F(CustomLayoutTest, ZIndexOrdersViewsAcrossAForEach) {
  const Laid result =
      laid(components::ZStack(
               components::ForEach(
                   std::views::iota(0, 2), [](int index) { return index; },
                   [](int index) {
                     return box(index == 0 ? "red" : "green", 40.0F, 40.0F)
                         .z_index(index == 0 ? 2.0 : 0.0);
                   }),
               box("blue", 40.0F, 40.0F).z_index(1.0))
               .id("stack"));
  const auto stack = result.tree->find(composition::ElementKey{"stack"});
  ASSERT_TRUE(stack.has_value());
  const composition::Node& parent = result.tree->node(*stack)->get();
  ASSERT_TRUE(composition::PaintOrder::reorders(parent));
  std::vector<const composition::Node*> order;
  composition::PaintOrder::arrange(parent, order);
  ASSERT_EQ(order.size(), 3U);
  const auto keyed = [&result](std::string_view key) {
    return &result.tree->node(*result.tree->find(composition::ElementKey{key}))
                ->get();
  };
  EXPECT_EQ(order[0], keyed("green"));
  EXPECT_EQ(order[1], keyed("blue"));
  EXPECT_EQ(order[2], keyed("red"));
  EXPECT_TRUE(composition::PaintOrder::above(*result.tree, keyed("red")->id(),
                                             keyed("blue")->id()));
  EXPECT_FALSE(composition::PaintOrder::above(
      *result.tree, keyed("green")->id(), keyed("blue")->id()));
}

TEST_F(CustomLayoutTest, GlobalFramesInScrolledContentFollowTheOffset) {
  auto seen = std::make_shared<std::vector<float>>();
  const Laid result = laid(
      components::ScrollView(
          components::VStack(
              box("top", 40.0F, 100.0F),
              box("probe", 40.0F, 40.0F)
                  .on_geometry_change(
                      std::type_identity<float>{},
                      [](const composition::GeometryProxy& proxy) {
                        return proxy.frame(composition::CoordinateSpace::global)
                            .y;
                      },
                      [seen](float y) { seen->push_back(y); }),
              box("bottom", 40.0F, 400.0F)))
          .frame({.width = 200.0F, .height = 200.0F})
          .id("scroll"));
  const float resting = frame(*result.tree, "probe").y;
  ASSERT_FALSE(seen->empty());
  EXPECT_NEAR(seen->back(), resting, tolerance);
  const auto scroll = result.tree->find(composition::ElementKey{"scroll"});
  ASSERT_TRUE(scroll.has_value());
  for (const float offset : {50.0F, 120.0F, 0.0F, 200.0F}) {
    result.tree->present_scroll(
        *scroll,
        composition::ScrollPresentation{.offset = {.x = 0.0F, .y = offset}});
    EXPECT_NEAR(seen->back(), resting - offset, tolerance) << offset;
  }
  const std::size_t notified = seen->size();
  result.tree->present_scroll(*scroll, composition::ScrollPresentation{
                                           .offset = {.x = 0.0F, .y = 200.0F}});
  EXPECT_EQ(seen->size(), notified);
}

TEST_F(CustomLayoutTest, ScrollingRenotifiesWithoutRelayout) {
  auto seen = std::make_shared<std::vector<composition::Rect>>();
  auto sized = std::make_shared<int>(0);
  const Laid result =
      laid(components::ScrollView(
               components::VStack(
                   box("top", 40.0F, 100.0F),
                   box("probe", 40.0F, 40.0F)
                       .on_geometry_change(
                           std::type_identity<composition::Rect>{},
                           [](const composition::GeometryProxy& proxy) {
                             return proxy.frame(
                                 composition::CoordinateSpace::scroll_view);
                           },
                           [seen](composition::Rect frame) {
                             seen->push_back(frame);
                           })
                       .on_geometry_change(
                           std::type_identity<composition::Size>{},
                           [](const composition::GeometryProxy& proxy) {
                             return proxy.size();
                           },
                           [sized](composition::Size) { ++*sized; }),
                   box("bottom", 40.0F, 400.0F)))
               .frame({.width = 200.0F, .height = 200.0F})
               .id("scroll"));
  const auto scroll = result.tree->find(composition::ElementKey{"scroll"});
  ASSERT_TRUE(scroll.has_value());
  ASSERT_FALSE(seen->empty());
  const float resting = seen->back().y;
  EXPECT_NEAR(resting,
              frame(*result.tree, "probe").y - frame(*result.tree, "scroll").y,
              tolerance);
  const int sizes = *sized;
  result.tree->present_scroll(*scroll, composition::ScrollPresentation{
                                           .offset = {.x = 0.0F, .y = 70.0F}});
  EXPECT_NEAR(seen->back().y, resting - 70.0F, tolerance);
  EXPECT_EQ(*sized, sizes);
  EXPECT_FALSE(result.tree->dirty());
}

TEST_F(CustomLayoutTest, SpacesInsideTheScrolledContentStayContentRelative) {
  auto inside = std::make_shared<float>(-1.0F);
  auto outside = std::make_shared<float>(-1.0F);
  const Laid result =
      laid(components::VStack(
               box("header", 40.0F, 30.0F),
               components::ScrollView(
                   components::VStack(
                       box("top", 40.0F, 100.0F),
                       box("probe", 40.0F, 40.0F)
                           .on_geometry_change(
                               std::type_identity<float>{},
                               [](const composition::GeometryProxy& proxy) {
                                 return proxy
                                     .frame(composition::CoordinateSpace::named(
                                         "content"))
                                     .y;
                               },
                               [inside](float y) { *inside = y; })
                           .on_geometry_change(
                               std::type_identity<float>{},
                               [](const composition::GeometryProxy& proxy) {
                                 return proxy
                                     .frame(composition::CoordinateSpace::named(
                                         "page"))
                                     .y;
                               },
                               [outside](float y) { *outside = y; }),
                       box("bottom", 40.0F, 400.0F))
                       .coordinate_space("content"))
                   .frame({.width = 200.0F, .height = 200.0F})
                   .id("scroll"))
               .coordinate_space("page")
               .id("page"));
  const float content_relative = *inside;
  const float page_relative = *outside;
  EXPECT_NEAR(page_relative,
              frame(*result.tree, "probe").y - frame(*result.tree, "page").y,
              tolerance);
  EXPECT_GT(content_relative, 99.0F);
  EXPECT_LT(content_relative, page_relative);
  result.tree->present_scroll(
      *result.tree->find(composition::ElementKey{"scroll"}),
      composition::ScrollPresentation{.offset = {.x = 0.0F, .y = 90.0F}});
  EXPECT_NEAR(*inside, content_relative, tolerance);
  EXPECT_NEAR(*outside, page_relative - 90.0F, tolerance);
}

TEST_F(CustomLayoutTest, AGeometryReaderParallaxFollowsTheScroll) {
  auto parallax = std::make_shared<float>(0.0F);
  const Laid result = laid(
      components::ScrollView(
          components::VStack(
              components::GeometryReader(
                  [parallax](const composition::GeometryProxy& proxy) {
                    *parallax =
                        -proxy.frame(composition::CoordinateSpace::global).y *
                        0.5F;
                    return composition::Element{box("hero", 40.0F, 40.0F)};
                  })
                  .frame({.height = 120.0F}),
              box("bottom", 40.0F, 600.0F)))
          .frame({.width = 200.0F, .height = 200.0F})
          .id("scroll"));
  const float resting = *parallax;
  result.tree->present_scroll(
      *result.tree->find(composition::ElementKey{"scroll"}),
      composition::ScrollPresentation{.offset = {.x = 0.0F, .y = 80.0F}});
  result.tree->reconcile();
  EXPECT_NEAR(*parallax - resting, 40.0F, tolerance);
}

TEST_F(CustomLayoutTest, AnchorsInScrolledContentFollowTheOffset) {
  static const composition::PreferenceKey<
      std::optional<composition::Anchor<composition::Rect>>>
      marker{"marker", std::nullopt,
             [](std::optional<composition::Anchor<composition::Rect>>& value,
                std::optional<composition::Anchor<composition::Rect>> next) {
               if (next.has_value()) {
                 value = next;
               }
             }};
  const Laid result = laid(
      components::ScrollView(
          components::VStack(
              box("top", 40.0F, 100.0F),
              box("probe", 40.0F, 40.0F)
                  .anchor_preference(
                      marker,
                      composition::AnchorSource<composition::Rect>::bounds(),
                      [](const composition::Anchor<composition::Rect>& anchor) {
                        return std::optional{anchor};
                      })))
          .frame({.width = 200.0F, .height = 200.0F})
          .id("scroll"));
  const auto scroll = result.tree->find(composition::ElementKey{"scroll"});
  ASSERT_TRUE(scroll.has_value());
  const composition::Node& viewport = result.tree->node(*scroll)->get();
  const composition::GeometryProxy origin{};
  const float resting = frame(*result.tree, "probe").y;
  const auto before = viewport.preference(marker);
  ASSERT_TRUE(before.has_value() && before->has_value());
  EXPECT_NEAR(origin[**before].y, resting, tolerance);
  result.tree->present_scroll(*scroll, composition::ScrollPresentation{
                                           .offset = {.x = 0.0F, .y = 50.0F}});
  const auto after = viewport.preference(marker);
  ASSERT_TRUE(after.has_value() && after->has_value());
  EXPECT_NEAR(origin[**after].y, resting - 50.0F, tolerance);
}

class Directed {
 public:
  explicit Directed(
      std::shared_ptr<std::vector<composition::LayoutDirection>> seen)
      : seen_(std::move(seen)) {}

  auto size_that_fits(ProposedViewSize, const LayoutSubviews& subviews) const
      -> composition::Size {
    seen_->push_back(subviews.layout_direction());
    EXPECT_TRUE(subviews == subviews.subrange(0, subviews.size()));
    EXPECT_EQ(subviews.subrange(1, 1).layout_direction(),
              subviews.layout_direction());
    EXPECT_FALSE(subviews == subviews.subrange(0, 1));
    EXPECT_TRUE(subviews[0] == subviews[0]);
    EXPECT_FALSE(subviews[0] == subviews[1]);
    return composition::Size{.width = 40.0F, .height = 10.0F};
  }

  void place_subviews(composition::Rect bounds, ProposedViewSize,
                      const LayoutSubviews& subviews) const {
    for (const components::LayoutSubview& subview : subviews) {
      subview.place(composition::Point{.x = bounds.x, .y = bounds.y});
    }
  }

  friend auto operator==(const Directed&, const Directed&) -> bool = default;

 private:
  std::shared_ptr<std::vector<composition::LayoutDirection>> seen_;
};

TEST_F(CustomLayoutTest, SubviewsReportTheDirectionTheLayoutRunsIn) {
  for (const composition::LayoutDirection direction :
       composition::all_layout_directions) {
    auto seen = std::make_shared<std::vector<composition::LayoutDirection>>();
    const Laid result = laid(
        components::AnyLayout(Directed{seen})(box("a", 10.0F, 10.0F),
                                              box("b", 10.0F, 10.0F))
            .environment(components::Environment::layout_direction, direction)
            .id("directed"));
    ASSERT_FALSE(seen->empty());
    EXPECT_EQ(seen->back(), direction);
  }
}

TEST_F(CustomLayoutTest, NestedCustomGuidesCombineTheirExplicitValues) {
  static constexpr composition::AlignmentID combined{
      "combined", composition::Axis::horizontal,
      [](const ViewDimensions& dimensions) {
        return dimensions[HorizontalAlignment::leading];
      }};
  const Laid custom = laid(
      components::VStack(
          components::VStackOptions{.spacing = theme::Space{0.0F},
                                    .guide = combined},
          box("anchor", 100.0F, 10.0F),
          components::VStack(
              components::VStackOptions{
                  .alignment = HorizontalAlignment::leading,
                  .spacing = theme::Space{0.0F}},
              box("a", 40.0F, 10.0F)
                  .alignment_guide(combined,
                                   [](const ViewDimensions&) { return 10.0F; }),
              box("b", 40.0F, 10.0F)
                  .alignment_guide(combined,
                                   [](const ViewDimensions&) { return 30.0F; }))
              .id("inner"))
          .id("outer"));
  EXPECT_NEAR(offset(*custom.tree, "inner", "outer").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*custom.tree, "anchor", "outer").x, 20.0F, tolerance);
  const Laid leading = laid(
      components::VStack(
          components::VStackOptions{.alignment = HorizontalAlignment::leading,
                                    .spacing = theme::Space{0.0F}},
          box("anchor", 100.0F, 10.0F),
          components::VStack(
              components::VStackOptions{
                  .alignment = HorizontalAlignment::leading,
                  .spacing = theme::Space{0.0F}},
              box("a", 40.0F, 10.0F)
                  .alignment_guide(HorizontalAlignment::leading,
                                   [](const ViewDimensions&) { return 10.0F; }),
              box("b", 40.0F, 10.0F)
                  .alignment_guide(HorizontalAlignment::leading,
                                   [](const ViewDimensions&) { return 30.0F; }))
              .id("inner"))
          .id("outer"));
  EXPECT_NEAR(offset(*leading.tree, "inner", "outer").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*leading.tree, "anchor", "outer").x, 0.0F, tolerance);
}

TEST_F(CustomLayoutTest, CombineExplicitReducesLikeSwiftUI) {
  const std::array<std::optional<float>, 3> values{10.0F, std::nullopt, 30.0F};
  EXPECT_FALSE(composition::AlignmentKey{HorizontalAlignment::leading}
                   .combine_explicit(values)
                   .has_value());
  EXPECT_FALSE(composition::AlignmentKey{VerticalAlignment::top}
                   .combine_explicit(values)
                   .has_value());
  EXPECT_EQ(composition::AlignmentKey{VerticalAlignment::first_text_baseline}
                .combine_explicit(values),
            10.0F);
  EXPECT_EQ(composition::AlignmentKey{VerticalAlignment::last_text_baseline}
                .combine_explicit(values),
            30.0F);
  EXPECT_EQ(composition::AlignmentKey{colon}.combine_explicit(values), 20.0F);
  const std::array<std::optional<float>, 1> none{std::nullopt};
  EXPECT_FALSE(
      composition::AlignmentKey{colon}.combine_explicit(none).has_value());
  static constexpr composition::AlignmentID widest{
      "widest", composition::Axis::horizontal,
      [](const ViewDimensions&) { return 0.0F; },
      [](std::span<const std::optional<float>> found) -> std::optional<float> {
        std::optional<float> most;
        for (const std::optional<float>& each : found) {
          if (each.has_value() && (!most.has_value() || *each > *most)) {
            most = each;
          }
        }
        return most;
      }};
  EXPECT_EQ(composition::AlignmentKey{widest}.combine_explicit(values), 30.0F);
}

TEST_F(CustomLayoutTest, ViewSpacingUnionsOnlyTheChosenEdges) {
  const components::ViewSpacing narrow{2.0F, 0.0F, false};
  const components::ViewSpacing wide{20.0F, 0.0F, false};
  const components::ViewSpacing sideways =
      narrow.union_with(wide, theme::Edge::horizontal);
  EXPECT_FLOAT_EQ(sideways.distance(narrow, composition::Axis::horizontal),
                  20.0F);
  EXPECT_FLOAT_EQ(sideways.distance(narrow, composition::Axis::vertical), 2.0F);
  EXPECT_FLOAT_EQ(narrow.distance(sideways, composition::Axis::horizontal),
                  20.0F);
  components::ViewSpacing below = narrow;
  below.form_union(wide, theme::Edge::bottom);
  EXPECT_FLOAT_EQ(below.distance(narrow, composition::Axis::vertical), 20.0F);
  EXPECT_FLOAT_EQ(narrow.distance(below, composition::Axis::vertical), 2.0F);
  EXPECT_FLOAT_EQ(below.distance(narrow, composition::Axis::horizontal), 2.0F);
  EXPECT_EQ(narrow.union_with(wide), narrow.union_with(wide, theme::Edge::all));
  EXPECT_NE(narrow.union_with(wide, theme::Edge::top), narrow.union_with(wide));
}

TEST_F(CustomLayoutTest, ViewDimensionsCompareSizesBaselinesAndGuides) {
  const ViewDimensions plain{40.0F, 10.0F};
  EXPECT_EQ(plain, (ViewDimensions{40.0F, 10.0F}));
  EXPECT_NE(plain, (ViewDimensions{41.0F, 10.0F}));
  EXPECT_NE(plain, (ViewDimensions{40.0F, 10.0F,
                                   composition::TextBaselines{.first = 8.0F}}));
  const auto fixed = [](const ViewDimensions&) { return 5.0F; };
  const std::array guides{composition::AlignmentGuide{colon, fixed}};
  const ViewDimensions guided{40.0F, 10.0F, composition::TextBaselines{},
                              guides};
  EXPECT_NE(plain, guided);
  EXPECT_EQ(guided, (ViewDimensions{40.0F, 10.0F, composition::TextBaselines{},
                                    guides}));
}

}

#include <arisa/components/divider.hpp>
#include <arisa/components/grid.hpp>
#include <arisa/components/grid_layout.hpp>
#include <arisa/components/grid_row.hpp>
#include <arisa/components/layout_properties.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/subviews_collection_slice.hpp>

#include <cstdint>
#include <type_traits>

namespace arisa::layout {

TEST_F(CustomLayoutTest, ARecolouredThemeIsReadFreshWhenTheLayoutIsKept) {
  auto step = std::make_shared<composition::Observable<int>>(0);
  auto built = std::make_unique<composition::Tree>(Recoloured{step},
                                                   [] { return true; });
  built->reconcile();
  auto context = LayoutContext::create();
  ASSERT_TRUE(context.has_value());
  const auto settle = [&] {
    for (int round = 0; round < 3; ++round) {
      EXPECT_TRUE(context->update(*built, handheld, std::ref(typesetter()))
                      .has_value());
      if (!built->dirty()) {
        return;
      }
      built->reconcile();
    }
  };
  settle();
  const float gap = theme::Theme::defaults().space(theme::space::lg);
  const auto measured = [&] {
    return frame(*built, "b").x - (frame(*built, "a").x + 30.0F);
  };
  EXPECT_NEAR(measured(), gap, tolerance);
  step->set(1);
  built->reconcile();
  settle();
  EXPECT_NEAR(frame(*built, "lead").width, 80.0F, tolerance);
  EXPECT_NEAR(frame(*built, "row").width, 320.0F, tolerance);
  EXPECT_NEAR(measured(), gap, tolerance);
}

class LayoutProtocolTest : public CustomLayoutTest {
 protected:
  template <class Inner>
  class Delegate {
   public:
    explicit Delegate(Inner inner) : inner_(inner) {}

    auto size_that_fits(ProposedViewSize proposal,
                        const LayoutSubviews& subviews) const
        -> composition::Size {
      return inner_.size_that_fits(proposal, subviews);
    }

    void place_subviews(composition::Rect bounds, ProposedViewSize proposal,
                        const LayoutSubviews& subviews) const {
      inner_.place_subviews(bounds, proposal, subviews);
    }

    friend auto operator==(const Delegate&, const Delegate&) -> bool = default;

   private:
    Inner inner_;
  };

  struct Read {
    float first{0.0F};
    float last{0.0F};
    float height{0.0F};
  };

  class Reader {
   public:
    explicit Reader(std::shared_ptr<std::vector<Read>> seen)
        : seen_(std::move(seen)) {}

    auto size_that_fits(ProposedViewSize, const LayoutSubviews& subviews) const
        -> composition::Size {
      seen_->clear();
      composition::Size total{};
      for (const components::LayoutSubview& subview : subviews) {
        const ViewDimensions dimensions =
            subview.dimensions(ProposedViewSize::unspecified);
        seen_->push_back(
            Read{.first = dimensions[VerticalAlignment::first_text_baseline],
                 .last = dimensions[VerticalAlignment::last_text_baseline],
                 .height = dimensions.height()});
        total.width = std::max(total.width, dimensions.width());
        total.height += dimensions.height();
      }
      return total;
    }

    void place_subviews(composition::Rect bounds, ProposedViewSize,
                        const LayoutSubviews& subviews) const {
      float y = bounds.y;
      for (const components::LayoutSubview& subview : subviews) {
        subview.place(composition::Point{.x = bounds.x, .y = y});
        y += subview.size_that_fits(ProposedViewSize::unspecified).height;
      }
    }

    friend auto operator==(const Reader&, const Reader&) -> bool = default;

   private:
    std::shared_ptr<std::vector<Read>> seen_;
  };

  class Column {
   public:
    struct Options {
      bool zero_spacing{false};
      std::optional<float> leading{};
      std::optional<float> top{};

      friend auto operator==(const Options&, const Options&) -> bool = default;
    };

    Column() : options_(Options{}) {}

    explicit Column(Options options) : options_(options) {}

    auto spacing(const LayoutSubviews& subviews) const
        -> components::ViewSpacing {
      return options_.zero_spacing ? components::ViewSpacing::zero
                                   : subviews.spacing();
    }

    auto size_that_fits(ProposedViewSize, const LayoutSubviews& subviews) const
        -> composition::Size {
      composition::Size total{};
      for (const components::LayoutSubview& subview : subviews) {
        const composition::Size size =
            subview.size_that_fits(ProposedViewSize::unspecified);
        total.width = std::max(total.width, size.width);
        total.height += size.height;
      }
      return total;
    }

    void place_subviews(composition::Rect bounds, ProposedViewSize,
                        const LayoutSubviews& subviews) const {
      float y = bounds.y;
      for (const components::LayoutSubview& subview : subviews) {
        subview.place(composition::Point{.x = bounds.x, .y = y});
        y += subview.size_that_fits(ProposedViewSize::unspecified).height;
      }
    }

    auto explicit_alignment(const composition::AlignmentKey& guide,
                            composition::Rect bounds, ProposedViewSize,
                            const LayoutSubviews&) const
        -> std::optional<float> {
      if (guide == composition::AlignmentKey{HorizontalAlignment::leading} &&
          options_.leading.has_value()) {
        return bounds.x + *options_.leading;
      }
      if (guide == composition::AlignmentKey{VerticalAlignment::top} &&
          options_.top.has_value()) {
        return bounds.y + *options_.top;
      }
      return std::nullopt;
    }

    friend auto operator==(const Column&, const Column&) -> bool = default;

   private:
    Options options_;
  };

  class Plain {
   public:
    auto size_that_fits(ProposedViewSize, const LayoutSubviews& subviews) const
        -> composition::Size {
      return Column{}.size_that_fits(ProposedViewSize::unspecified, subviews);
    }

    void place_subviews(composition::Rect bounds, ProposedViewSize proposal,
                        const LayoutSubviews& subviews) const {
      Column{}.place_subviews(bounds, proposal, subviews);
    }

    friend auto operator==(const Plain&, const Plain&) -> bool = default;
  };

  struct Probe {
    composition::Size unspecified;
    composition::Size zero;
    composition::Size hundred;
  };

  enum class Orientation : std::uint8_t {
    horizontal,
    vertical,
    none,
  };

  template <Orientation Kind>
  class Oriented {
   public:
    static constexpr components::LayoutProperties layout_properties{
        .stack_orientation = Kind == Orientation::horizontal
                                 ? std::optional{composition::Axis::horizontal}
                             : Kind == Orientation::vertical
                                 ? std::optional{composition::Axis::vertical}
                                 : std::nullopt};

    explicit Oriented(std::shared_ptr<std::vector<Probe>> seen)
        : seen_(std::move(seen)) {}

    auto size_that_fits(ProposedViewSize, const LayoutSubviews& subviews) const
        -> composition::Size {
      seen_->clear();
      for (const components::LayoutSubview& subview : subviews) {
        seen_->push_back(Probe{
            .unspecified =
                subview.size_that_fits(ProposedViewSize::unspecified),
            .zero = subview.size_that_fits(ProposedViewSize::zero),
            .hundred =
                subview.size_that_fits(ProposedViewSize{100.0F, 100.0F})});
      }
      return composition::Size{.width = 10.0F, .height = 10.0F};
    }

    void place_subviews(composition::Rect, ProposedViewSize,
                        const LayoutSubviews&) const {}

    friend auto operator==(const Oriented&, const Oriented&) -> bool = default;

   private:
    std::shared_ptr<std::vector<Probe>> seen_;
  };

  static constexpr composition::ContainerValueKey<int> order{"order", 0};

  class Ordered {
   public:
    auto size_that_fits(ProposedViewSize, const LayoutSubviews& subviews) const
        -> composition::Size {
      composition::Size total{};
      for (const components::LayoutSubview& subview : subviews) {
        const composition::Size size =
            subview.size_that_fits(ProposedViewSize::unspecified);
        total.width += size.width;
        total.height = std::max(total.height, size.height);
      }
      return total;
    }

    void place_subviews(composition::Rect bounds, ProposedViewSize,
                        const LayoutSubviews& subviews) const {
      std::vector<std::size_t> sorted(subviews.size());
      for (std::size_t index = 0; index < sorted.size(); ++index) {
        sorted[index] = index;
      }
      std::ranges::stable_sort(sorted, {}, [&subviews](std::size_t index) {
        const composition::KeyedValues& values =
            subviews[index].container_values();
        return values.get(order) * 10 + values.tag<int>().value_or(0);
      });
      float x = bounds.x;
      for (const std::size_t index : sorted) {
        subviews[index].place(composition::Point{.x = x, .y = bounds.y});
        x +=
            subviews[index].size_that_fits(ProposedViewSize::unspecified).width;
      }
    }

    friend auto operator==(const Ordered&, const Ordered&) -> bool = default;
  };

  struct Counted {
    std::size_t count{0};
    std::size_t slice{0};
    std::optional<int> tag{};
    bool has_tag{false};
    bool untagged{false};
  };

  class Pair final : public composition::Component {
   public:
    auto operator==(const Pair&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      return components::Group(box("p0", 10.0F, 10.0F),
                               box("p1", 20.0F, 10.0F));
    }
  };

  class Counter final : public composition::Component {
   public:
    explicit Counter(std::shared_ptr<Counted> seen) : seen_(std::move(seen)) {}

    auto operator==(const Counter&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return components::Group(
          components::SubviewsCollection{
              context,
              components::Group(Pair{}, box("p2", 30.0F, 10.0F).tag(3))},
          [this](components::SubviewsCollection subviews) {
            seen_->count = subviews.size();
            seen_->slice = subviews.slice(1).size();
            seen_->tag = subviews[2].container_values().tag<int>();
            seen_->has_tag = subviews[2].container_values().has_tag(3);
            seen_->untagged =
                !subviews[0].container_values().tag<int>().has_value();
            return components::HStack(
                       components::HStackOptions{.spacing = theme::Space{0.0F}},
                       components::ForEach(subviews.slice(1),
                                           [](components::Subview subview) {
                                             return std::move(subview).view();
                                           }))
                .id("row");
          });
    }

   private:
    std::shared_ptr<Counted> seen_;
  };

  static auto fill(std::string_view key, composition::FrameSpec spec)
      -> composition::Element {
    return components::VStack()
        .frame(spec)
        .background(theme::ColourRole::accent, theme::Shape::rectangle())
        .id(composition::ElementKey{key});
  }

  static auto rtl(composition::Element content) -> composition::Element {
    return std::move(content).environment(
        components::Environment::layout_direction,
        composition::LayoutDirection::right_to_left);
  }
};

TEST_F(LayoutProtocolTest, BuiltInStacksMeasureAndPlaceInsideACustomLayout) {
  const Laid row = laid(components::AnyLayout(Delegate{components::HStackLayout{
      components::HStackOptions{.alignment = VerticalAlignment::top,
                                .spacing = theme::Space{4.0F}}}})(
                            box("a", 30.0F, 20.0F), box("b", 40.0F, 10.0F))
                            .id("stack"));
  EXPECT_NEAR(offset(*row.tree, "b", "stack").x, 34.0F, tolerance);
  EXPECT_NEAR(offset(*row.tree, "b", "stack").y, 0.0F, tolerance);
  EXPECT_NEAR(frame(*row.tree, "stack").width, 74.0F, tolerance);
  EXPECT_NEAR(frame(*row.tree, "stack").height, 20.0F, tolerance);
  const Laid spaced =
      laid(components::AnyLayout(Delegate{components::HStackLayout{}})(
               box("a", 30.0F, 10.0F), box("b", 30.0F, 10.0F))
               .id("stack"));
  EXPECT_NEAR(offset(*spaced.tree, "b", "stack").x, 38.0F, tolerance);
  const Laid flexible =
      laid(components::AnyLayout(Delegate{components::HStackLayout{
          components::HStackOptions{.spacing = theme::Space{0.0F}}}})(
               fill("a", {.height = 10.0F,
                          .min_width = 10.0F,
                          .max_width = composition::infinity}),
               fill("b", {.height = 10.0F, .max_width = 40.0F}),
               box("c", 50.0F, 10.0F))
               .frame({.width = 200.0F})
               .id("stack"));
  EXPECT_NEAR(frame(*flexible.tree, "a").width, 110.0F, tolerance);
  EXPECT_NEAR(frame(*flexible.tree, "b").width, 40.0F, tolerance);
  EXPECT_NEAR(offset(*flexible.tree, "c", "stack").x, 150.0F, tolerance);
  const Laid column =
      laid(components::AnyLayout(Delegate{components::VStackLayout{
          components::VStackOptions{.alignment = HorizontalAlignment::trailing,
                                    .spacing = theme::Space{6.0F}}}})(
               box("a", 30.0F, 20.0F), box("b", 50.0F, 10.0F))
               .id("stack"));
  EXPECT_NEAR(offset(*column.tree, "a", "stack").x, 20.0F, tolerance);
  EXPECT_NEAR(offset(*column.tree, "b", "stack").y, 26.0F, tolerance);
  EXPECT_NEAR(frame(*column.tree, "stack").width, 50.0F, tolerance);
  EXPECT_NEAR(frame(*column.tree, "stack").height, 36.0F, tolerance);
}

TEST_F(LayoutProtocolTest,
       BuiltInZStackAndGridMeasureAndPlaceInsideACustomLayout) {
  const Laid layered = laid(components::AnyLayout(Delegate{
      components::ZStackLayout{components::ZStackOptions{
          .alignment = components::Alignment::bottom_trailing}}})(
                                box("a", 30.0F, 20.0F), box("b", 50.0F, 10.0F))
                                .id("stack"));
  EXPECT_NEAR(offset(*layered.tree, "a", "stack").x, 20.0F, tolerance);
  EXPECT_NEAR(offset(*layered.tree, "a", "stack").y, 0.0F, tolerance);
  EXPECT_NEAR(offset(*layered.tree, "b", "stack").y, 10.0F, tolerance);
  EXPECT_NEAR(frame(*layered.tree, "stack").width, 50.0F, tolerance);
  EXPECT_NEAR(frame(*layered.tree, "stack").height, 20.0F, tolerance);
  const Laid grid = laid(components::AnyLayout(Delegate{components::GridLayout{
      components::GridOptions{.alignment = components::Alignment::leading,
                              .horizontal_spacing = theme::Space{4.0F},
                              .vertical_spacing = theme::Space{2.0F}}}})(
                             box("a", 30.0F, 10.0F), box("b", 50.0F, 20.0F))
                             .id("stack"));
  EXPECT_NEAR(offset(*grid.tree, "a", "stack").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*grid.tree, "b", "stack").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*grid.tree, "b", "stack").y, 12.0F, tolerance);
  EXPECT_NEAR(frame(*grid.tree, "stack").width, 50.0F, tolerance);
  EXPECT_NEAR(frame(*grid.tree, "stack").height, 32.0F, tolerance);
}

TEST_F(LayoutProtocolTest, SubviewDimensionsReportTextBaselines) {
  auto seen = std::make_shared<std::vector<Read>>();
  const Laid result = laid(components::AnyLayout(Reader{seen})(
      components::Text("One\nTwo"), box("box", 30.0F, 20.0F),
      components::VStack(
          components::VStackOptions{.spacing = theme::Space{0.0F}},
          components::Text("A"), components::Text("B")),
      components::Text("One\nTwo").padding(theme::Insets::all(5.0F)),
      components::Text("One")));
  ASSERT_EQ(seen->size(), 5U);
  const Read& two = seen->at(0);
  const Read& single = seen->at(4);
  const float line = single.height;
  EXPECT_GT(two.first, 0.0F);
  EXPECT_LT(two.first, line);
  EXPECT_NEAR(two.last - two.first, two.height - line, 1.0F);
  EXPECT_LT(two.last, two.height);
  EXPECT_NEAR(seen->at(1).first, 20.0F, tolerance);
  EXPECT_NEAR(seen->at(1).last, 20.0F, tolerance);
  EXPECT_NEAR(seen->at(2).first, two.first, tolerance);
  EXPECT_NEAR(seen->at(2).last, two.last, 1.0F);
  EXPECT_NEAR(seen->at(3).first, two.first + 5.0F, tolerance);
  EXPECT_NEAR(seen->at(3).last, two.last + 5.0F, tolerance);
  EXPECT_NEAR(seen->at(3).height, two.height + 10.0F, tolerance);
  EXPECT_NEAR(single.first, single.last, tolerance);
  EXPECT_NEAR(single.first, two.first, tolerance);
}

TEST_F(LayoutProtocolTest, ZStackHonoursAlignmentGuides) {
  const Laid leading =
      laid(components::ZStack(
               components::ZStackOptions{.alignment =
                                             components::Alignment::leading},
               box("a", 40.0F, 10.0F)
                   .alignment_guide(
                       HorizontalAlignment::leading,
                       [](const ViewDimensions& dimensions) {
                         return dimensions[HorizontalAlignment::trailing];
                       }),
               box("b", 60.0F, 10.0F))
               .id("stack"));
  EXPECT_NEAR(offset(*leading.tree, "a", "stack").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*leading.tree, "b", "stack").x, 40.0F, tolerance);
  EXPECT_NEAR(frame(*leading.tree, "stack").width, 100.0F, tolerance);
  const Laid corner = laid(
      components::ZStack(
          components::ZStackOptions{.alignment =
                                        components::Alignment::top_leading},
          box("a", 40.0F, 20.0F)
              .alignment_guide(VerticalAlignment::top,
                               [](const ViewDimensions&) { return 5.0F; })
              .alignment_guide(HorizontalAlignment::leading,
                               [](const ViewDimensions&) { return 10.0F; }),
          box("b", 60.0F, 30.0F))
          .id("stack"));
  EXPECT_NEAR(offset(*corner.tree, "a", "stack").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*corner.tree, "a", "stack").y, 0.0F, tolerance);
  EXPECT_NEAR(offset(*corner.tree, "b", "stack").x, 10.0F, tolerance);
  EXPECT_NEAR(offset(*corner.tree, "b", "stack").y, 5.0F, tolerance);
  EXPECT_NEAR(frame(*corner.tree, "stack").width, 70.0F, tolerance);
  EXPECT_NEAR(frame(*corner.tree, "stack").height, 35.0F, tolerance);
  const Laid custom =
      laid(components::ZStack(
               components::ZStackOptions{.horizontal_guide = colon},
               box("a", 40.0F, 10.0F)
                   .alignment_guide(
                       colon,
                       [](const ViewDimensions& dimensions) {
                         return dimensions[HorizontalAlignment::trailing];
                       }),
               box("b", 60.0F, 10.0F))
               .id("stack"));
  EXPECT_NEAR(offset(*custom.tree, "a", "stack").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*custom.tree, "b", "stack").x, 40.0F, tolerance);
  EXPECT_NEAR(frame(*custom.tree, "stack").width, 100.0F, tolerance);
  const Laid mirrored =
      laid(rtl(components::ZStack(
                   components::ZStackOptions{
                       .alignment = components::Alignment::leading},
                   box("a", 40.0F, 10.0F)
                       .alignment_guide(
                           HorizontalAlignment::leading,
                           [](const ViewDimensions& dimensions) {
                             return dimensions[HorizontalAlignment::trailing];
                           }),
                   box("b", 60.0F, 10.0F))
                   .id("stack")));
  EXPECT_NEAR(offset(*mirrored.tree, "a", "stack").x, 60.0F, tolerance);
  EXPECT_NEAR(offset(*mirrored.tree, "b", "stack").x, 0.0F, tolerance);
  EXPECT_NEAR(frame(*mirrored.tree, "stack").width, 100.0F, tolerance);
}

TEST_F(LayoutProtocolTest, GridHonoursAlignmentGuides) {
  const Laid leading = laid(
      components::Grid(
          components::GridOptions{.alignment = components::Alignment::leading,
                                  .horizontal_spacing = theme::Space{0.0F},
                                  .vertical_spacing = theme::Space{0.0F}},
          components::GridRow(
              box("a", 40.0F, 10.0F)
                  .alignment_guide(HorizontalAlignment::leading,
                                   [](const ViewDimensions&) { return 10.0F; }),
              box("c", 20.0F, 10.0F)),
          components::GridRow(box("b", 60.0F, 10.0F), box("d", 20.0F, 10.0F)))
          .id("grid"));
  EXPECT_NEAR(offset(*leading.tree, "a", "grid").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*leading.tree, "b", "grid").x, 10.0F, tolerance);
  EXPECT_NEAR(offset(*leading.tree, "c", "grid").x, 60.0F, tolerance);
  EXPECT_NEAR(offset(*leading.tree, "d", "grid").x, 60.0F, tolerance);
  EXPECT_NEAR(frame(*leading.tree, "grid").width, 80.0F, tolerance);
  const Laid top =
      laid(components::Grid(
               components::GridOptions{.alignment = components::Alignment::top,
                                       .horizontal_spacing = theme::Space{0.0F},
                                       .vertical_spacing = theme::Space{0.0F}},
               components::GridRow(
                   box("a", 20.0F, 40.0F),
                   box("b", 20.0F, 20.0F)
                       .alignment_guide(
                           VerticalAlignment::top,
                           [](const ViewDimensions& dimensions) {
                             return dimensions[VerticalAlignment::bottom];
                           })))
               .id("grid"));
  EXPECT_NEAR(offset(*top.tree, "a", "grid").y, 20.0F, tolerance);
  EXPECT_NEAR(offset(*top.tree, "b", "grid").y, 0.0F, tolerance);
  EXPECT_NEAR(frame(*top.tree, "grid").height, 40.0F, tolerance);
}

TEST_F(LayoutProtocolTest, GuidedStacksMirrorRightToLeft) {
  const auto trailing = [](const ViewDimensions& dimensions) {
    return dimensions[HorizontalAlignment::trailing];
  };
  const Laid column = laid(rtl(
      components::VStack(
          components::VStackOptions{.alignment = HorizontalAlignment::leading,
                                    .spacing = theme::Space{0.0F}},
          box("a", 40.0F, 10.0F)
              .alignment_guide(HorizontalAlignment::leading, trailing),
          box("b", 60.0F, 10.0F),
          box("c", 30.0F, 10.0F)
              .alignment_guide(HorizontalAlignment::leading,
                               [](const ViewDimensions&) { return 15.0F; }))
          .id("stack")));
  EXPECT_NEAR(offset(*column.tree, "a", "stack").x, 60.0F, tolerance);
  EXPECT_NEAR(offset(*column.tree, "b", "stack").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*column.tree, "c", "stack").x, 45.0F, tolerance);
  EXPECT_NEAR(frame(*column.tree, "stack").width, 100.0F, tolerance);
  const Laid row = laid(rtl(
      components::HStack(
          components::HStackOptions{.alignment = VerticalAlignment::top,
                                    .spacing = theme::Space{0.0F}},
          box("a", 20.0F, 40.0F),
          box("b", 30.0F, 20.0F)
              .alignment_guide(VerticalAlignment::top,
                               [](const ViewDimensions& dimensions) {
                                 return dimensions[VerticalAlignment::bottom];
                               }))
          .id("row")));
  EXPECT_NEAR(offset(*row.tree, "a", "row").x, 30.0F, tolerance);
  EXPECT_NEAR(offset(*row.tree, "a", "row").y, 20.0F, tolerance);
  EXPECT_NEAR(offset(*row.tree, "b", "row").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*row.tree, "b", "row").y, 0.0F, tolerance);
  const auto field = [](std::string_view label, std::string_view name,
                        float width) {
    return components::HStack(
        components::HStackOptions{.spacing = theme::Space{4.0F}},
        box(label, width, 10.0F),
        box(name, 50.0F, 10.0F)
            .alignment_guide(colon, [](const ViewDimensions& dimensions) {
              return dimensions[HorizontalAlignment::leading];
            }));
  };
  const Laid form =
      laid(rtl(components::VStack(
                   components::VStackOptions{.spacing = theme::Space{0.0F},
                                             .guide = colon},
                   field("l0", "f0", 30.0F), field("l1", "f1", 70.0F))
                   .id("form")));
  EXPECT_NEAR(offset(*form.tree, "l0", "form").x, 54.0F, tolerance);
  EXPECT_NEAR(offset(*form.tree, "f0", "form").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*form.tree, "l1", "form").x, 54.0F, tolerance);
  EXPECT_NEAR(offset(*form.tree, "f1", "form").x, 0.0F, tolerance);
  EXPECT_NEAR(frame(*form.tree, "form").width, 124.0F, tolerance);
}

TEST_F(LayoutProtocolTest, AComponentBodyExpandsIntoItsSubviews) {
  auto seen = std::make_shared<Counted>();
  const Laid result = laid(Counter{seen});
  EXPECT_EQ(seen->count, 3U);
  EXPECT_EQ(seen->slice, 2U);
  EXPECT_EQ(seen->tag, std::optional<int>{3});
  EXPECT_TRUE(seen->has_tag);
  EXPECT_TRUE(seen->untagged);
  EXPECT_NEAR(frame(*result.tree, "row").width, 50.0F, tolerance);
  EXPECT_FALSE(present(*result.tree, "p0"));
}

TEST_F(LayoutProtocolTest, SubviewsCollectionSlicesNarrowTheCollection) {
  components::SubviewsCollection subviews{components::Group(
      box("a", 10.0F, 10.0F), box("b", 10.0F, 10.0F), box("c", 10.0F, 10.0F))};
  const components::SubviewsCollectionSlice tail = subviews.slice(1);
  ASSERT_EQ(tail.size(), 2U);
  EXPECT_EQ(tail[0].id(), composition::ElementKey{"b"});
  EXPECT_EQ(tail.slice(1)[0].id(), composition::ElementKey{"c"});
  EXPECT_TRUE(subviews.slice(1, 1).empty());
  EXPECT_EQ(subviews.slice(5).size(), 0U);
  const Laid result = laid(
      components::HStack(
          components::HStackOptions{.spacing = theme::Space{0.0F}},
          composition::Element{components::SubviewsCollection{
              components::Group(box("x", 10.0F, 10.0F), box("y", 20.0F, 10.0F),
                                box("z", 30.0F, 10.0F))}
                                   .slice(1)})
          .id("row"));
  EXPECT_FALSE(present(*result.tree, "x"));
  EXPECT_NEAR(frame(*result.tree, "row").width, 50.0F, tolerance);
}

TEST_F(LayoutProtocolTest, LayoutSubviewsCarryContainerValuesAndTags) {
  const Laid result = laid(components::AnyLayout(Ordered{})(
                               box("a", 10.0F, 10.0F).container_value(order, 2),
                               box("b", 20.0F, 10.0F).tag(1),
                               box("c", 30.0F, 10.0F).container_value(order, 1))
                               .id("row"));
  EXPECT_NEAR(offset(*result.tree, "b", "row").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "c", "row").x, 20.0F, tolerance);
  EXPECT_NEAR(offset(*result.tree, "a", "row").x, 50.0F, tolerance);
  composition::KeyedValues values;
  EXPECT_FALSE(values.tag<int>().has_value());
  values.set_tag(composition::TagValue::of(7));
  EXPECT_EQ(values.tag(std::type_identity<int>{}), std::optional<int>{7});
  EXPECT_FALSE(values.tag<float>().has_value());
  EXPECT_TRUE(values.has_tag(7));
  EXPECT_FALSE(values.has_tag(8));
}

TEST_F(LayoutProtocolTest, ACustomLayoutSpacingIsWhatNeighboursSee) {
  const Laid zero = laid(components::HStack(
      components::AnyLayout(Column{Column::Options{.zero_spacing = true}})(
          box("inner", 20.0F, 10.0F))
          .id("custom"),
      box("b", 20.0F, 10.0F)));
  EXPECT_NEAR(frame(*zero.tree, "b").x - (frame(*zero.tree, "custom").x +
                                          frame(*zero.tree, "custom").width),
              0.0F, tolerance);
  const auto gap = [](const Laid& result) {
    const composition::Rect top = frame(*result.tree, "top");
    return frame(*result.tree, "below").y - (top.y + top.height);
  };
  const Laid united = laid(components::VStack(
      components::AnyLayout(Column{})(components::Text("A")).id("top"),
      components::Text("B").id("below")));
  const Laid texts = laid(components::VStack(
      components::Text("A").id("top"), components::Text("B").id("below")));
  EXPECT_NEAR(gap(united), gap(texts), tolerance);
  const Laid boxed = laid(components::VStack(
      components::AnyLayout(Column{})(box("inner", 20.0F, 10.0F)).id("top"),
      components::Text("B").id("below")));
  const Laid plain = laid(components::VStack(
      box("top", 20.0F, 10.0F), components::Text("B").id("below")));
  EXPECT_NEAR(gap(boxed), gap(plain), tolerance);
}

TEST_F(LayoutProtocolTest, ExplicitAlignmentAnswersOnTheCustomContainer) {
  const Laid leading = laid(
      components::VStack(
          components::VStackOptions{.alignment = HorizontalAlignment::leading,
                                    .spacing = theme::Space{0.0F}},
          components::AnyLayout(Column{Column::Options{.leading = 25.0F}})(
              box("inner", 40.0F, 10.0F))
              .id("custom"),
          box("b", 60.0F, 10.0F))
          .id("stack"));
  EXPECT_NEAR(offset(*leading.tree, "custom", "stack").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*leading.tree, "b", "stack").x, 25.0F, tolerance);
  EXPECT_NEAR(frame(*leading.tree, "stack").width, 85.0F, tolerance);
  const Laid top =
      laid(components::HStack(
               components::HStackOptions{.alignment = VerticalAlignment::top,
                                         .spacing = theme::Space{0.0F}},
               components::AnyLayout(Column{Column::Options{.top = 10.0F}})(
                   box("inner", 20.0F, 20.0F))
                   .id("custom"),
               box("b", 20.0F, 40.0F))
               .id("row"));
  EXPECT_NEAR(offset(*top.tree, "custom", "row").y, 0.0F, tolerance);
  EXPECT_NEAR(offset(*top.tree, "b", "row").y, 10.0F, tolerance);
  EXPECT_NEAR(frame(*top.tree, "row").height, 50.0F, tolerance);
  const Laid merged = laid(
      components::VStack(
          components::VStackOptions{.alignment = HorizontalAlignment::leading,
                                    .spacing = theme::Space{0.0F}},
          components::AnyLayout(Plain{})(
              box("inner", 40.0F, 10.0F)
                  .alignment_guide(HorizontalAlignment::leading,
                                   [](const ViewDimensions&) { return 10.0F; }))
              .id("custom"),
          box("b", 60.0F, 10.0F))
          .id("stack"));
  EXPECT_NEAR(offset(*merged.tree, "custom", "stack").x, 0.0F, tolerance);
  EXPECT_NEAR(offset(*merged.tree, "b", "stack").x, 0.0F, tolerance);
}

TEST_F(LayoutProtocolTest, StackOrientationShapesSpacersAndDividers) {
  const float minimum = 8.0F;
  auto across = std::make_shared<std::vector<Probe>>();
  const Laid horizontal =
      laid(components::AnyLayout(Oriented<Orientation::horizontal>{across})(
          components::Spacer(), components::Divider()));
  ASSERT_EQ(across->size(), 2U);
  EXPECT_NEAR(across->at(0).unspecified.width, minimum, tolerance);
  EXPECT_NEAR(across->at(0).unspecified.height, 0.0F, tolerance);
  EXPECT_NEAR(across->at(0).zero.width, minimum, tolerance);
  EXPECT_NEAR(across->at(0).hundred.width, 100.0F, tolerance);
  EXPECT_NEAR(across->at(0).hundred.height, 0.0F, tolerance);
  EXPECT_NEAR(across->at(1).hundred.width, 1.0F, tolerance);
  EXPECT_NEAR(across->at(1).hundred.height, 100.0F, tolerance);
  auto down = std::make_shared<std::vector<Probe>>();
  const Laid vertical =
      laid(components::AnyLayout(Oriented<Orientation::vertical>{down})(
          components::Spacer(), components::Divider()));
  ASSERT_EQ(down->size(), 2U);
  EXPECT_NEAR(down->at(0).unspecified.height, minimum, tolerance);
  EXPECT_NEAR(down->at(0).unspecified.width, 0.0F, tolerance);
  EXPECT_NEAR(down->at(0).hundred.height, 100.0F, tolerance);
  EXPECT_NEAR(down->at(0).hundred.width, 0.0F, tolerance);
  EXPECT_NEAR(down->at(1).hundred.height, 1.0F, tolerance);
  EXPECT_NEAR(down->at(1).hundred.width, 100.0F, tolerance);
  auto loose = std::make_shared<std::vector<Probe>>();
  const Laid none =
      laid(components::AnyLayout(Oriented<Orientation::none>{loose})(
          components::Spacer(), components::Divider()));
  ASSERT_EQ(loose->size(), 2U);
  EXPECT_NEAR(loose->at(0).unspecified.width, minimum, tolerance);
  EXPECT_NEAR(loose->at(0).unspecified.height, minimum, tolerance);
  EXPECT_NEAR(loose->at(0).zero.width, minimum, tolerance);
  EXPECT_NEAR(loose->at(0).zero.height, minimum, tolerance);
  EXPECT_NEAR(loose->at(0).hundred.width, 100.0F, tolerance);
  EXPECT_NEAR(loose->at(0).hundred.height, 100.0F, tolerance);
  EXPECT_NEAR(loose->at(1).hundred.height, 1.0F, tolerance);
  EXPECT_NEAR(loose->at(1).hundred.width, 100.0F, tolerance);
  EXPECT_EQ(components::LayoutProperties{}.stack_orientation, std::nullopt);
}

TEST_F(LayoutProtocolTest, AnIdealQueryAnswersWithTheFrameIdealLikeSwiftUI) {
  auto seen = std::make_shared<std::vector<Probe>>();
  const Laid probed = laid(components::AnyLayout(
      Oriented<Orientation::none>{seen})(
      components::ZStack().frame(
          {.ideal_width = 80.0F, .max_width = composition::infinity}),
      components::ZStack().frame(
          {.ideal_height = 30.0F, .max_height = composition::infinity}),
      components::ZStack().frame({.ideal_width = 50.0F, .ideal_height = 20.0F}),
      components::ZStack().frame({.min_width = 10.0F,
                                  .ideal_width = 60.0F,
                                  .max_width = 200.0F,
                                  .min_height = 5.0F,
                                  .ideal_height = 25.0F,
                                  .max_height = 100.0F})));
  ASSERT_EQ(seen->size(), 4U);
  EXPECT_NEAR(seen->at(0).unspecified.width, 80.0F, tolerance);
  EXPECT_NEAR(seen->at(0).unspecified.height, 0.0F, tolerance);
  EXPECT_NEAR(seen->at(0).hundred.width, 100.0F, tolerance);
  EXPECT_NEAR(seen->at(1).unspecified.width, 0.0F, tolerance);
  EXPECT_NEAR(seen->at(1).unspecified.height, 30.0F, tolerance);
  EXPECT_NEAR(seen->at(1).hundred.height, 100.0F, tolerance);
  EXPECT_NEAR(seen->at(2).unspecified.width, 50.0F, tolerance);
  EXPECT_NEAR(seen->at(2).unspecified.height, 20.0F, tolerance);
  EXPECT_NEAR(seen->at(3).unspecified.width, 60.0F, tolerance);
  EXPECT_NEAR(seen->at(3).unspecified.height, 25.0F, tolerance);
  EXPECT_NEAR(seen->at(3).zero.width, 10.0F, tolerance);
  EXPECT_NEAR(seen->at(3).zero.height, 5.0F, tolerance);
}

TEST_F(LayoutProtocolTest, ContainerSizingOptionsTellUniformFromVariable) {
  using Options = components::Subview::ContainerSizingOptions;
  const Options uniform = Options::uniform(composition::Axes::horizontal);
  EXPECT_TRUE(uniform.is_uniform());
  EXPECT_EQ(uniform.axes(), composition::Axes::horizontal);
  EXPECT_FALSE(Options::variable.is_uniform());
  EXPECT_TRUE(Options::uniform(composition::Axes{}).is_uniform());
  EXPECT_NE(Options::uniform(composition::Axes{}), Options::variable);
  EXPECT_NE(uniform, Options::uniform(composition::Axes::all));
}

}
