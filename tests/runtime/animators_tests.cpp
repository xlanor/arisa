#include <arisa/components/environment.hpp>
#include <arisa/components/glass_effect_container.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/timeline_schedule.hpp>
#include <arisa/components/timeline_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/geometry_effect.hpp>
#include <arisa/composition/glass_effect.hpp>
#include <arisa/composition/keyframe_animator.hpp>
#include <arisa/composition/keyframe_repeat.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/phase_animator.hpp>
#include <arisa/composition/symbol_effect.hpp>
#include <arisa/composition/symbol_effect_animator.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/projection_transform.hpp>
#include <arisa/graphics/shader.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/keyframes.hpp>
#include <arisa/motion/linear_keyframe.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/transaction_key.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Element;

class AnimatorsTest : public FocusFixture {
 protected:
  static void step(provider::headless::HeadlessProviderFactory& providers,
                   Application& application,
                   std::chrono::nanoseconds duration = frame_step) {
    ASSERT_TRUE(providers.clock().advance(duration).has_value());
    frames(application, 1);
  }

  static void load_size(
      provider::headless::HeadlessProviderFactory& providers) {
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{1280},
                                     .height = provider::SurfaceHeight{720}};
    providers.surface().resize(size);
    providers.gpu().rasterize(size);
  }

  static auto box(float width, std::string key) -> Element {
    return components::ZStack()
        .frame({.width = width, .height = 20.0F})
        .background(theme::Colour::hex(0x204060))
        .id(std::move(key));
  }

  static auto shown(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    if (!node.has_value()) {
      return composition::Rect{};
    }
    return node->get().presented().has_value() ? node->get().presented()->bounds
                                               : node->get().frame();
  }

  static auto deepest(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    if (!node.has_value()) {
      return composition::Rect{};
    }
    const composition::Node* at = &node->get();
    while (at->child_count() == 1) {
      at = &*at->children().begin();
    }
    return at->presented().has_value() ? at->presented()->bounds : at->frame();
  }

  static auto presentation(Application& application, std::string_view key)
      -> motion::Presentation& {
    const auto id = node_of(application, key);
    composition::Tree& tree = application.root().tree();
    return tree.presentation(tree.node(*id)->get());
  }

  struct Width {
    double value{0.0};
  };

  class Bar final : public composition::Component {
   public:
    using AnimatableData = double;

    explicit Bar(double width) : width_(width) {}

    auto operator==(const Bar&) const -> bool = default;

    auto animatable_data() const -> double { return width_; }

    void set_animatable_data(double width) { width_ = width; }

    auto body(composition::Context&) const -> Element override {
      return box(static_cast<float>(width_), "bar");
    }

   private:
    double width_;
  };

  struct Skew {
    double amount{0.0};

    using AnimatableData = double;

    auto animatable_data() const -> double { return amount; }

    void set_animatable_data(double value) { amount = value; }

    auto effect_value(composition::Size) const
        -> graphics::ProjectionTransform {
      return graphics::ProjectionTransform{
          std::array<float, 9>{1.0F, 0.0F, 0.0F, static_cast<float>(amount),
                               1.0F, 0.0F, 0.0F, 0.0F, 1.0F}};
    }

    friend auto operator==(const Skew&, const Skew&) -> bool = default;
  };
};

TEST_F(AnimatorsTest, RepeatingPhasesAdvanceAtEachLogicalCompletion) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<std::vector<int>>();
  Application application = start(
      providers, Element{composition::PhaseAnimator<int>{
                     std::vector<int>{0, 1, 2},
                     [seen](const int& phase) {
                       if (seen->empty() || seen->back() != phase) {
                         seen->push_back(phase);
                       }
                       return box(100.0F + (static_cast<float>(phase) * 50.0F),
                                  "phase");
                     },
                     [](const int&) -> std::optional<motion::Animation> {
                       return motion::Animation::linear(0.2);
                     }}});
  ASSERT_FALSE(seen->empty());
  EXPECT_EQ(seen->front(), 0);
  step(providers, application);
  EXPECT_EQ(seen->back(), 1);
  step(providers, application, 100ms);
  EXPECT_EQ(seen->back(), 1);
  EXPECT_GT(shown(application, "phase").width, 100.0F);
  EXPECT_LT(shown(application, "phase").width, 150.0F);
  step(providers, application, 100ms);
  EXPECT_EQ(seen->back(), 2);
  step(providers, application, 200ms);
  EXPECT_EQ(seen->back(), 0);
  step(providers, application, 200ms);
  EXPECT_EQ(seen->back(), 1);
  EXPECT_EQ(*seen, (std::vector<int>{0, 1, 2, 0, 1}));
}

TEST_F(AnimatorsTest, TriggeredPhasesRunOnceAndComeToRest) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<int> trigger{0};
  auto seen = std::make_shared<std::vector<int>>();
  class Host final : public composition::Component {
   public:
    Host(const composition::Observable<int>& trigger,
         std::shared_ptr<std::vector<int>> seen)
        : trigger_(trigger), seen_(std::move(seen)) {}

    auto operator==(const Host& other) const -> bool { return this == &other; }

    auto body(composition::Context& context) const -> Element override {
      const int fired = context.read(trigger_.get());
      return box(40.0F, "anchor")
          .phase_animator(
              std::vector<int>{0, 1, 2}, fired,
              [seen = seen_](Element content, const int& phase) {
                if (seen->empty() || seen->back() != phase) {
                  seen->push_back(phase);
                }
                return std::move(content).offset(
                    static_cast<float>(phase) * 10.0F, 0.0F);
              },
              [](const int&) -> std::optional<motion::Animation> {
                return motion::Animation::linear(0.2);
              });
    }

   private:
    std::reference_wrapper<const composition::Observable<int>> trigger_;
    std::shared_ptr<std::vector<int>> seen_;
  };
  Application application = start(providers, Host{trigger, seen});
  for (int index = 0; index < 5; ++index) {
    step(providers, application);
  }
  EXPECT_EQ(*seen, (std::vector<int>{0}));
  EXPECT_EQ(application.last_frame().kind, FrameKind::idle);
  trigger.set(1);
  step(providers, application);
  step(providers, application);
  EXPECT_EQ(seen->back(), 1);
  step(providers, application, 200ms);
  EXPECT_EQ(seen->back(), 2);
  step(providers, application, 200ms);
  EXPECT_EQ(seen->back(), 0);
  step(providers, application, 400ms);
  step(providers, application, 400ms);
  EXPECT_EQ(*seen, (std::vector<int>{0, 1, 2, 0}));
  step(providers, application);
  EXPECT_EQ(application.last_frame().kind, FrameKind::idle);
}

TEST_F(AnimatorsTest, RepeatingKeyframesLoopTheirTimeline) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            box(40.0F, "looped")
                .keyframe_animator(
                    Width{0.0}, composition::KeyframeRepeat{},
                    [](Element content, const Width& width) {
                      return std::move(content).offset(
                          static_cast<float>(width.value), 0.0F);
                    },
                    [](const Width&) {
                      return motion::Keyframes{motion::KeyframeTrack(
                          &Width::value, motion::LinearKeyframe(100.0, 1.0))};
                    }));
  step(providers, application, 250ms);
  const float quarter = deepest(application, "looped").x;
  step(providers, application, 1000ms);
  EXPECT_NEAR(deepest(application, "looped").x, quarter, 1.0F);
  step(providers, application, 500ms);
  EXPECT_NEAR(deepest(application, "looped").x, quarter + 50.0F, 1.0F);
}

TEST_F(AnimatorsTest, AnimatableComponentsInterpolateTheirData) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<double> width{100.0};
  class Host final : public composition::Component {
   public:
    explicit Host(const composition::Observable<double>& width)
        : width_(width) {}

    auto operator==(const Host& other) const -> bool { return this == &other; }

    auto body(composition::Context& context) const -> Element override {
      return Bar{context.read(width_.get())};
    }

   private:
    std::reference_wrapper<const composition::Observable<double>> width_;
  };
  Application application = start(providers, Host{width});
  EXPECT_FLOAT_EQ(node_named(application, "bar")->get().frame().width, 100.0F);
  motion::with_animation(motion::Animation::linear(1.0),
                         [&width] { width.set(300.0); });
  step(providers, application);
  step(providers, application, 500ms);
  EXPECT_NEAR(node_named(application, "bar")->get().frame().width, 200.0F,
              4.0F);
  step(providers, application, 600ms);
  step(providers, application);
  EXPECT_FLOAT_EQ(node_named(application, "bar")->get().frame().width, 300.0F);
  width.set(120.0);
  step(providers, application);
  EXPECT_FLOAT_EQ(node_named(application, "bar")->get().frame().width, 120.0F);
}

TEST_F(AnimatorsTest, GeometryEffectsAnimateTheirData) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<double> skew{0.0};
  class Host final : public composition::Component {
   public:
    explicit Host(const composition::Observable<double>& skew) : skew_(skew) {}

    auto operator==(const Host& other) const -> bool { return this == &other; }

    auto body(composition::Context& context) const -> Element override {
      return box(60.0F, "skewed")
          .geometry_effect(Skew{.amount = context.read(skew_.get())});
    }

   private:
    std::reference_wrapper<const composition::Observable<double>> skew_;
  };
  Application application = start(providers, Host{skew});
  motion::with_animation(motion::Animation::linear(1.0),
                         [&skew] { skew.set(0.5); });
  step(providers, application);
  step(providers, application, 500ms);
  EXPECT_EQ(application.last_frame().kind, FrameKind::update);
  step(providers, application, 600ms);
  step(providers, application);
  step(providers, application);
  EXPECT_EQ(application.last_frame().kind, FrameKind::idle);
}

TEST_F(AnimatorsTest, ValueScopedTransactionsAndScopedAnimations) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<bool> wide{false};
  composition::Observable<int> trigger{0};
  class Host final : public composition::Component {
   public:
    Host(const composition::Observable<bool>& wide,
         const composition::Observable<int>& trigger)
        : wide_(wide), trigger_(trigger) {}

    auto operator==(const Host& other) const -> bool { return this == &other; }

    auto body(composition::Context& context) const -> Element override {
      const bool wide = context.read(wide_.get());
      const int fired = context.read(trigger_.get());
      return components::VStack(
          box(wide ? 300.0F : 100.0F, "scoped")
              .transaction(fired,
                           [](motion::Transaction& transaction) {
                             transaction.set_animation(
                                 motion::Animation::linear(1.0));
                           }),
          components::VStack(box(wide ? 300.0F : 100.0F, "inner"))
              .animation(motion::Animation::linear(1.0),
                         [wide](Element content) {
                           return std::move(content).opacity(wide ? 0.5F
                                                                  : 1.0F);
                         })
              .id("outer"));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> wide_;
    std::reference_wrapper<const composition::Observable<int>> trigger_;
  };
  Application application = start(providers, Host{wide, trigger});
  wide.set(true);
  trigger.set(1);
  step(providers, application);
  step(providers, application, 500ms);
  EXPECT_NEAR(shown(application, "scoped").width, 200.0F, 6.0F);
  EXPECT_FLOAT_EQ(shown(application, "inner").width, 300.0F);
  EXPECT_TRUE(presentation(application, "outer")
                  .in_flight(motion::Presentation::Property::opacity));
  step(providers, application, 600ms);
  wide.set(false);
  step(providers, application);
  step(providers, application);
  EXPECT_FLOAT_EQ(shown(application, "scoped").width, 100.0F);
}

TEST_F(AnimatorsTest, TransactionKeysReachTheirReaders) {
  provider::headless::HeadlessProviderFactory providers;
  static constexpr motion::TransactionKey<int> origin{"origin", 0};
  auto read = std::make_shared<int>(-1);
  composition::Observable<int> trigger{0};
  class Reader final : public composition::Component {
   public:
    Reader(const composition::Observable<int>& trigger,
           std::shared_ptr<int> read)
        : trigger_(trigger), read_(std::move(read)) {}

    auto operator==(const Reader& other) const -> bool {
      return this == &other;
    }

    auto body(composition::Context& context) const -> Element override {
      static_cast<void>(context.read(trigger_.get()));
      *read_ = context.transaction()[origin];
      return box(10.0F, "reader");
    }

   private:
    std::reference_wrapper<const composition::Observable<int>> trigger_;
    std::shared_ptr<int> read_;
  };
  Application application = start(providers, Reader{trigger, read});
  EXPECT_EQ(*read, 0);
  motion::Transaction tagged;
  tagged.set(origin, 4);
  motion::with_transaction(tagged, [&trigger] { trigger.set(1); });
  step(providers, application);
  EXPECT_EQ(*read, 4);
}

TEST_F(AnimatorsTest, PeriodicTimelinesWakeOnlyAtTheirEntries) {
  provider::headless::HeadlessProviderFactory providers;
  auto entries = std::make_shared<int>(0);
  Application application = start(
      providers,
      Element{components::TimelineView(
          components::TimelineSchedule::periodic(
              components::TimelineSchedule::Date{}, 1s),
          [entries](const components::TimelineView::Context& timeline) {
            ++*entries;
            return Element{components::Text(
                std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                                   timeline.date.time_since_epoch())
                                   .count()))};
          })});
  const int first = *entries;
  int idle = 0;
  for (int index = 0; index < 20; ++index) {
    step(providers, application, 20ms);
    idle += application.last_frame().kind == FrameKind::idle ? 1 : 0;
  }
  EXPECT_EQ(*entries, first);
  EXPECT_EQ(idle, 20);
  step(providers, application, 1s);
  EXPECT_EQ(*entries, first + 1);
}

TEST_F(AnimatorsTest, PausedAnimationTimelinesStayIdle) {
  provider::headless::HeadlessProviderFactory providers;
  auto entries = std::make_shared<int>(0);
  Application application =
      start(providers, Element{components::TimelineView(
                           components::TimelineSchedule::animation(0ns, true),
                           [entries](const components::TimelineView::Context&) {
                             ++*entries;
                             return box(10.0F, "paused");
                           })});
  const int first = *entries;
  for (int index = 0; index < 5; ++index) {
    step(providers, application);
  }
  EXPECT_EQ(*entries, first);
  EXPECT_EQ(application.last_frame().kind, FrameKind::idle);
}

TEST_F(AnimatorsTest, SymbolBounceRunsOncePerValueAndPulseWhileActive) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<int> taps{0};
  composition::Observable<bool> pulsing{false};
  class Host final : public composition::Component {
   public:
    Host(const composition::Observable<int>& taps,
         const composition::Observable<bool>& pulsing)
        : taps_(taps), pulsing_(pulsing) {}

    auto operator==(const Host& other) const -> bool { return this == &other; }

    auto body(composition::Context& context) const -> Element override {
      return components::HStack(
          box(30.0F, "heart")
              .symbol_effect(composition::SymbolEffect::bounce(),
                             context.read(taps_.get())),
          box(30.0F, "dot")
              .symbol_effect(composition::SymbolEffect::pulse(),
                             context.read(pulsing_.get())));
    }

   private:
    std::reference_wrapper<const composition::Observable<int>> taps_;
    std::reference_wrapper<const composition::Observable<bool>> pulsing_;
  };
  Application application = start(providers, Host{taps, pulsing});
  const composition::Rect resting = deepest(application, "heart");
  taps.set(1);
  step(providers, application);
  step(providers, application, 100ms);
  EXPECT_LT(deepest(application, "heart").y, resting.y - 1.0F);
  step(providers, application, 1s);
  step(providers, application);
  EXPECT_NEAR(deepest(application, "heart").y, resting.y, 0.01F);
  EXPECT_EQ(application.last_frame().kind, FrameKind::idle);
  pulsing.set(true);
  step(providers, application);
  step(providers, application, 250ms);
  EXPECT_TRUE(presentation(application, "dot")
                  .in_flight(motion::Presentation::Property::opacity));
  EXPECT_TRUE(application.last_frame().animating);
  pulsing.set(false);
  step(providers, application);
  step(providers, application, 1s);
  step(providers, application);
  EXPECT_FALSE(application.last_frame().animating);
}

TEST_F(AnimatorsTest, BlurReplaceBlursAndScalesTheInsertion) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<bool> shown_flag{false};
  class Host final : public composition::Component {
   public:
    explicit Host(const composition::Observable<bool>& flag) : flag_(flag) {}

    auto operator==(const Host& other) const -> bool { return this == &other; }

    auto body(composition::Context& context) const -> Element override {
      std::vector<Element> children;
      if (context.read(flag_.get())) {
        children.push_back(components::ZStack()
                               .frame({.width = 100.0F, .height = 100.0F})
                               .background(theme::Colour::hex(0x000000))
                               .transition(motion::Transition::blur_replace())
                               .id("replaced"));
      }
      return components::ZStack(components::ZStackOptions{},
                                std::move(children))
          .frame({.width = 300.0F, .height = 300.0F})
          .background(theme::Colour::hex(0xFFFFFF));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> flag_;
  };
  load_size(providers);
  Application application = start(providers, Host{shown_flag});
  motion::with_animation(motion::Animation::linear(1.0),
                         [&shown_flag] { shown_flag.set(true); });
  step(providers, application);
  step(providers, application, 484ms);
  const composition::Rect laid =
      node_named(application, "replaced")->get().frame();
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const auto row = static_cast<std::uint32_t>(laid.y + (laid.height * 0.5F));
  const auto inside =
      (*image->pixel(static_cast<std::uint32_t>(laid.x + 50.0F), row))[0];
  const auto spill =
      (*image->pixel(static_cast<std::uint32_t>(laid.x) - 1U, row))[0];
  EXPECT_NEAR(static_cast<float>(inside), 255.0F * 0.5F, 20.0F);
  EXPECT_LT(spill, 250);
  step(providers, application, 600ms);
  step(providers, application);
  const auto settled = providers.gpu().capture();
  ASSERT_TRUE(settled.has_value());
  EXPECT_EQ((*settled->pixel(static_cast<std::uint32_t>(laid.x) - 2U, row))[0],
            255);
  EXPECT_EQ(
      (*settled->pixel(static_cast<std::uint32_t>(laid.x + 50.0F), row))[0], 0);
}

TEST_F(AnimatorsTest, BlurAndCornerChangesAnimateThroughMixes) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<bool> on{false};
  class Host final : public composition::Component {
   public:
    explicit Host(const composition::Observable<bool>& on) : on_(on) {}

    auto operator==(const Host& other) const -> bool { return this == &other; }

    auto body(composition::Context& context) const -> Element override {
      const bool on = context.read(on_.get());
      return components::VStack(
          box(80.0F, "blurred").blur(on ? 10.0F : 0.0F),
          components::ZStack()
              .frame({.width = 80.0F, .height = 60.0F})
              .background(
                  theme::Colour::hex(0x406080),
                  on ? theme::Shape{theme::UnevenRoundedRectangle{
                           theme::RectangleCornerRadii{.top_leading = 0.0F,
                                                       .bottom_leading = 20.0F,
                                                       .bottom_trailing = 0.0F,
                                                       .top_trailing = 20.0F}}}
                     : theme::Shape::rounded(theme::Radius{4.0F}))
              .id("cornered"),
          components::ZStack()
              .frame({.width = 80.0F, .height = 40.0F})
              .background(graphics::LinearGradient{
                  .gradient =
                      graphics::Gradient{std::vector<theme::ColourValue>{
                          theme::Colour::hex(on ? 0x00ff00 : 0xff0000),
                          theme::Colour::hex(on ? 0xffff00 : 0x0000ff)}},
                  .start_point = motion::UnitPoint::leading,
                  .end_point = motion::UnitPoint::trailing})
              .id("graded"));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> on_;
  };
  Application application = start(providers, Host{on});
  motion::with_animation(motion::Animation::linear(1.0),
                         [&on] { on.set(true); });
  step(providers, application);
  step(providers, application, 500ms);
  using Mix = motion::Presentation::Mix;
  const auto blur = presentation(application, "blurred").blend(Mix::blur);
  ASSERT_TRUE(blur.has_value());
  EXPECT_NEAR(blur->progress, 0.5F, 0.05F);
  EXPECT_TRUE(presentation(application, "cornered").in_flight(Mix::corners));
  EXPECT_TRUE(presentation(application, "graded").in_flight(Mix::fill));
  std::size_t graded = 0;
  for (const provider::ShapeVertex& vertex :
       providers.gpu().last_shapes().vertices) {
    graded += vertex.kind == provider::ShapeKind::gradient_fill ? 1U : 0U;
  }
  EXPECT_EQ(graded, 8U);
  step(providers, application, 600ms);
  step(providers, application);
  EXPECT_FALSE(presentation(application, "blurred").in_flight(Mix::blur));
  EXPECT_FALSE(presentation(application, "cornered").in_flight(Mix::corners));
  EXPECT_FALSE(presentation(application, "graded").in_flight(Mix::fill));
}

TEST_F(AnimatorsTest, GeometryGroupsCarryChildrenWithTheGroup) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<bool> wide{false};
  class Host final : public composition::Component {
   public:
    explicit Host(const composition::Observable<bool>& wide) : wide_(wide) {}

    auto operator==(const Host& other) const -> bool { return this == &other; }

    auto body(composition::Context& context) const -> Element override {
      const bool wide = context.read(wide_.get());
      std::vector<Element> children;
      if (wide) {
        children.push_back(
            box(20.0F, "late").transition(motion::Transition::identity()));
      }
      return components::HStack(
                 components::ZStack(
                     components::ZStackOptions{
                         .alignment = components::Alignment::top_leading},
                     std::move(children))
                     .frame({.width = wide ? 400.0F : 200.0F, .height = 50.0F})
                     .geometry_group()
                     .id("group"))
          .frame({.width = 600.0F, .height = 60.0F});
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> wide_;
  };
  Application application = start(providers, Host{wide});
  const float before = shown(application, "group").x;
  motion::with_animation(motion::Animation::linear(1.0),
                         [&wide] { wide.set(true); });
  step(providers, application);
  const float laid = node_named(application, "group")->get().frame().x;
  EXPECT_LT(laid, before);
  const float inset = node_named(application, "late")->get().frame().x - laid;
  EXPECT_NEAR(shown(application, "late").x, before + inset, 1.0F);
  step(providers, application, 500ms);
  EXPECT_NEAR(shown(application, "late").x, ((before + laid) * 0.5F) + inset,
              3.0F);
}

TEST_F(AnimatorsTest, GlassIdsMorphFrameAndCornersInAContainer) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<bool> expanded{false};
  static constexpr composition::Namespace glass_space{"glass-morph"};
  class Host final : public composition::Component {
   public:
    explicit Host(const composition::Observable<bool>& expanded)
        : expanded_(expanded) {}

    auto operator==(const Host& other) const -> bool { return this == &other; }

    auto body(composition::Context& context) const -> Element override {
      const composition::Namespace::ID space =
          context.namespace_id(glass_space);
      const bool open = context.read(expanded_.get());
      Element shape =
          open ? components::ZStack()
                     .frame({.width = 300.0F, .height = 44.0F})
                     .glass_effect(theme::Glass::regular(),
                                   theme::Shape::rounded(theme::Radius{12.0F}))
                     .glass_effect_id("morph", space)
                     .id("bar")
               : components::ZStack()
                     .frame({.width = 44.0F, .height = 44.0F})
                     .glass_effect(theme::Glass::regular(),
                                   theme::Shape::capsule())
                     .glass_effect_id("morph", space)
                     .id("button");
      return Element{components::GlassEffectContainer(std::move(shape))};
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> expanded_;
  };
  Application application = start(providers, Host{expanded});
  motion::with_animation(motion::Animation::linear(1.0),
                         [&expanded] { expanded.set(true); });
  step(providers, application);
  step(providers, application, 300ms);
  motion::Presentation& morphing = presentation(application, "bar");
  EXPECT_TRUE(morphing.morphing());
  EXPECT_TRUE(
      morphing.in_flight(motion::Presentation::Property::corner_radius));
  EXPECT_LT(shown(application, "bar").width, 300.0F);
}

TEST_F(AnimatorsTest, ReducedMotionHoldsRepeatingPhaseAnimators) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<std::vector<int>>();
  Application application =
      start(providers,
            Element{composition::PhaseAnimator<int>{std::vector<int>{0, 1},
                                                    [seen](const int& phase) {
                                                      seen->push_back(phase);
                                                      return box(50.0F, "held");
                                                    }}}
                .environment(components::Environment::reduce_motion, true));
  for (int index = 0; index < 10; ++index) {
    step(providers, application, 200ms);
  }
  for (const int phase : *seen) {
    EXPECT_EQ(phase, 0);
  }
  EXPECT_EQ(application.last_frame().kind, FrameKind::idle);
}

TEST_F(AnimatorsTest, HierarchicalLevelsAndMaterializeAnimate) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<bool> on{false};
  class Host final : public composition::Component {
   public:
    explicit Host(const composition::Observable<bool>& on) : on_(on) {}

    auto operator==(const Host& other) const -> bool { return this == &other; }

    auto body(composition::Context& context) const -> Element override {
      const bool on = context.read(on_.get());
      std::vector<Element> children;
      children.push_back(
          components::Text("Level")
              .foreground_style(on ? graphics::HierarchicalShapeStyle::tertiary
                                   : graphics::HierarchicalShapeStyle::primary)
              .id("level"));
      if (on) {
        children.push_back(
            components::Text("Glass")
                .padding(theme::space::md)
                .glass_effect(theme::Glass::regular(), theme::Shape::capsule())
                .glass_effect_transition(
                    composition::GlassEffectTransition::materialize())
                .id("materialized"));
      }
      return components::VStack(std::move(children));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> on_;
  };
  Application application = start(providers, Host{on});
  motion::with_animation(motion::Animation::linear(1.0),
                         [&on] { on.set(true); });
  step(providers, application);
  step(providers, application, 500ms);
  EXPECT_TRUE(presentation(application, "level")
                  .in_flight(motion::Presentation::Tint::foreground));
  const auto glass = node_named(application, "materialized");
  ASSERT_TRUE(glass.has_value());
  ASSERT_TRUE(glass->get().transition().has_value());
  EXPECT_FLOAT_EQ(glass->get().transition()->insertion().blur,
                  composition::GlassEffectTransition::materialize_blur);
  EXPECT_TRUE(presentation(application, "materialized").animating());
}

TEST_F(AnimatorsTest, VisualEffectsCarryShaders) {
  const composition::VisualEffect plain;
  EXPECT_FALSE(plain.graphic());
  const composition::VisualEffect shaded = plain.color_effect(graphics::Shader{
      graphics::ShaderFunction{0, graphics::ShaderFunction::Kind::colour}});
  EXPECT_TRUE(shaded.graphic());
  EXPECT_TRUE(shaded.shader().has_value());
  const composition::VisualEffect off = plain.layer_effect(
      graphics::Shader{
          graphics::ShaderFunction{1, graphics::ShaderFunction::Kind::layer}},
      composition::Size{.width = 4.0F, .height = 4.0F}, false);
  EXPECT_FALSE(off.shader().has_value());
}

}
