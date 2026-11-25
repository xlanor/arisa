#include <focus_fixture.hpp>

#include <arisa/async/manual_executor.hpp>
#include <arisa/async/task.hpp>
#include <arisa/components/canvas.hpp>
#include <arisa/components/content_unavailable_view.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/graphics_context.hpp>
#include <arisa/components/hint_bar.hpp>
#include <arisa/components/open_url_action.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/anchor.hpp>
#include <arisa/composition/anchor_source.hpp>
#include <arisa/composition/app_storage.hpp>
#include <arisa/composition/aspect_ratio.hpp>
#include <arisa/composition/background_task.hpp>
#include <arisa/composition/background_task_scheduler.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/environment_values.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/lazy_state.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/persistent_system_overlays.hpp>
#include <arisa/composition/preference_key.hpp>
#include <arisa/composition/preference_observer.hpp>
#include <arisa/composition/redaction_reasons.hpp>
#include <arisa/composition/scene_storage.hpp>
#include <arisa/composition/scroll_target_behavior_properties.hpp>
#include <arisa/composition/subscription_view.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/composition/task_options.hpp>
#include <arisa/composition/undo_manager.hpp>
#include <arisa/composition/user_defaults.hpp>
#include <arisa/composition/view_modifier.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/animatable_traits.hpp>
#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/animation_context.hpp>
#include <arisa/motion/animation_state.hpp>
#include <arisa/motion/animation_state_key.hpp>
#include <arisa/motion/custom_animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/memory_storage.hpp>
#include <arisa/runtime/benchmark_host.hpp>
#include <arisa/structure/assistive_access.hpp>
#include <arisa/structure/index_view_style.hpp>
#include <arisa/structure/page_index_view_style.hpp>
#include <arisa/theme/colour_mix.hpp>
#include <arisa/theme/rgb_color_space.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class AppDataParityTest : public FocusFixture {
 protected:
  using Element = composition::Element;

  struct Model {
    int count{0};

    friend auto operator==(const Model&, const Model&) -> bool = default;
  };

  static inline const composition::EnvironmentKey<int> number{"app-data.number",
                                                              0};
  static inline int animation_seen = 0;

  class Reading final : public motion::CustomAnimation {
   public:
    auto animate(const motion::AnimatableVector& value, double time,
                 motion::AnimationContext& context) const
        -> std::optional<motion::AnimatableVector> override {
      animation_seen = context.environment().size() == 0
                           ? -1
                           : context.environment()[number];
      if (time >= 0.2) {
        return std::nullopt;
      }
      return value * time;
    }
  };
  class Tagged final : public composition::ViewModifier {
   public:
    explicit Tagged(std::string key) : key_(std::move(key)) {}

    auto body(composition::Context&, Content content) const
        -> Element override {
      return components::VStack(std::move(content)).id(key_);
    }

    friend auto operator==(const Tagged&, const Tagged&) -> bool = default;

   private:
    std::string key_;
  };

  static auto with_files(provider::headless::HeadlessProviderFactory& providers,
                         Element content,
                         std::optional<provider::headless::MemoryStorage*> card,
                         LinkOptions links = {}) -> Application {
    StorageOptions storage{};
    std::optional<std::reference_wrapper<provider::StorageProvider>> files;
    if (card.has_value()) {
      storage.provider = **card;
      files = **card;
    }
    auto created =
        Application::create(providers, std::move(content),
                            ApplicationOptions{.executors = std::nullopt,
                                               .fonts = {},
                                               .atlas = {},
                                               .interaction = {},
                                               .storage = std::move(storage),
                                               .files = files,
                                               .links = std::move(links)});
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    frames(application, 3);
    return application;
  }
};

TEST_F(AppDataParityTest, EnvironmentValuesAreAFirstClassBag) {
  composition::EnvironmentValues values;
  EXPECT_EQ(values.size(), 0U);
  EXPECT_EQ(values[number], 0);
  values.set(number, 7);
  EXPECT_TRUE(values.contains(number));
  EXPECT_EQ(values[number], 7);
  composition::EnvironmentValues copy = values;
  copy.set(number, 9);
  EXPECT_EQ(values[number], 7);
  EXPECT_EQ(copy[number], 9);
  EXPECT_NE(copy, values);
  composition::Observable<Model> model{Model{.count = 3}};
  EXPECT_FALSE(values.object<Model>().has_value());
  values.set_object(model);
  ASSERT_TRUE(values.object<Model>().has_value());
  EXPECT_EQ(values.object<Model>()->get().get().count, 3);

  static std::optional<composition::EnvironmentValues> seen;
  class Reader final : public composition::Component {
   public:
    auto operator==(const Reader&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      seen = context.environment_values();
      return components::Text("read");
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, Element{Reader{}}.environment(number, 42));
  ASSERT_TRUE(seen.has_value());
  EXPECT_EQ((*seen)[number], 42);
  EXPECT_TRUE(seen->contains(components::Environment::locale));
}

TEST_F(AppDataParityTest, TypeKeyedObjectsReachEveryDescendant) {
  static composition::Observable<Model> model{Model{.count = 1}};
  class Reader final : public composition::Component {
   public:
    auto operator==(const Reader&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto found = context.environment_object<Model>();
      const int shown =
          found.has_value() ? context.read(found->get()).count : -1;
      return components::Text(std::to_string(shown)).id("count");
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::VStack(
                     Element{Reader{}}.environment(model),
                     Element{Reader{}}.id("object").environment_object(model)));
  EXPECT_TRUE(shows(application, "1"));
  model.set(Model{.count = 5});
  frames(application, 3);
  EXPECT_TRUE(shows(application, "5"));
  EXPECT_FALSE(shows(application, "-1"));
}

TEST_F(AppDataParityTest, TheInnermostRepeatedEnvironmentModifierWins) {
  class Reader final : public composition::Component {
   public:
    auto operator==(const Reader&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      return components::Text(std::to_string(context.environment(number)));
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            Element{Reader{}}.environment(number, 1).environment(number, 2));
  EXPECT_TRUE(shows(application, "1"));
  EXPECT_FALSE(shows(application, "2"));
}

TEST_F(AppDataParityTest, ContextsCarryTheirEnvironment) {
  static int canvas_seen = 0;
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            Element{components::Canvas([](components::GraphicsContext& context,
                                          composition::Size) {
              canvas_seen = context.environment()[number];
            })}.frame({.width = 40.0F, .height = 40.0F})
                .environment(number, 11));
  EXPECT_EQ(canvas_seen, 11);

  composition::EnvironmentValues values;
  values.set(number, 4);
  const composition::ScrollTargetBehaviorPropertiesContext scrolling{
      .axes = composition::Axes::vertical,
      .horizontally_compact = true,
      .environment = values};
  EXPECT_EQ(scrolling.environment[number], 4);

  static constexpr composition::StateKey<bool> faded{"app-data.faded", false};
  class Fading final : public composition::Component {
   public:
    auto operator==(const Fading&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto state = context.state(faded);
      return components::Button("fade", [state] { state.set(!state.get()); })
          .opacity(state.get() ? 0.2F : 1.0F)
          .animation(motion::Animation::custom(Reading{}), state.get())
          .frame({.width = 80.0F, .height = 40.0F})
          .id("fade");
    }
  };
  provider::headless::HeadlessProviderFactory animated;
  Application fading =
      start(animated, Element{Fading{}}.environment(number, 23));
  ASSERT_TRUE(fading.root().activate(composition::ElementKey{"fade"}));
  frames(fading, 4);
  EXPECT_EQ(animation_seen, 23);
}

TEST_F(AppDataParityTest, StorageTakesOptionalValuesAndPerPropertyStores) {
  provider::headless::MemoryStorage card;
  static composition::UserDefaults other;
  static const composition::AppStorage<std::optional<int>> maybe{"maybe"};
  static const composition::AppStorage<int> scoped{"scoped", 3, other};
  static const composition::AppStorage<std::optional<int>> scoped_maybe{
      "scoped-maybe", other};
  static const composition::SceneStorage<std::optional<std::string>> tab{"tab"};
  static const composition::SceneStorage<int> scene_scoped{"scene-scoped", 1,
                                                           other};
  static std::optional<composition::Binding<std::optional<int>>> maybe_bound;
  static std::optional<composition::Binding<int>> scoped_bound;
  static std::optional<composition::Binding<std::optional<std::string>>>
      tab_bound;
  class Reader final : public composition::Component {
   public:
    auto operator==(const Reader&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      maybe_bound = context.app_storage(maybe);
      scoped_bound = context.app_storage(scoped);
      tab_bound = context.scene_storage(tab);
      static_cast<void>(context.app_storage(scoped_maybe));
      static_cast<void>(context.scene_storage(scene_scoped));
      return components::Text("storage");
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      with_files(providers, Element{Reader{}}, std::optional{&card});
  ASSERT_TRUE(maybe_bound.has_value() && scoped_bound.has_value() &&
              tab_bound.has_value());
  EXPECT_FALSE(maybe_bound->get().has_value());
  maybe_bound->set(5);
  EXPECT_EQ(maybe_bound->get(), std::optional<int>{5});
  maybe_bound->set(std::nullopt);
  EXPECT_FALSE(maybe_bound->get().has_value());
  EXPECT_EQ(scoped_bound->get(), 3);
  scoped_bound->set(8);
  EXPECT_EQ(other.get<int>("scoped"), std::optional<int>{8});
  EXPECT_FALSE(tab_bound->get().has_value());
  tab_bound->set(std::string{"library"});
  EXPECT_EQ(tab_bound->get(), std::optional<std::string>{"library"});
}

TEST_F(AppDataParityTest, StorageReadersDependOnlyOnTheirOwnKey) {
  static int a_bodies = 0;
  static int b_bodies = 0;
  static const composition::AppStorage<int> first{"first", 0};
  static const composition::AppStorage<int> second{"second", 0};
  static std::optional<composition::Binding<int>> second_bound;
  class First final : public composition::Component {
   public:
    auto operator==(const First&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      ++a_bodies;
      return components::Text(std::to_string(context.app_storage(first).get()));
    }
  };
  class Second final : public composition::Component {
   public:
    auto operator==(const Second&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      ++b_bodies;
      second_bound = context.app_storage(second);
      return components::Text(std::to_string(second_bound->get()));
    }
  };
  provider::headless::MemoryStorage card;
  provider::headless::HeadlessProviderFactory providers;
  Application application = with_files(
      providers, components::VStack(Element{First{}}, Element{Second{}}),
      std::optional{&card});
  const int a_before = a_bodies;
  const int b_before = b_bodies;
  second_bound->set(4);
  frames(application, 3);
  EXPECT_EQ(a_bodies, a_before);
  EXPECT_GT(b_bodies, b_before);
  const int settled = b_bodies;
  frames(application, 5);
  EXPECT_EQ(b_bodies, settled);
}

TEST_F(AppDataParityTest, OnReceiveSubscribesToAPublisher) {
  static composition::Observable<int> ticks{0};
  static std::vector<int> received;
  static constexpr composition::StateKey<bool> shown{"app-data.receiving",
                                                     true};
  static std::optional<composition::StateHandle<bool>> toggle;
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      toggle = context.state(shown);
      if (!toggle->get()) {
        return components::Text("off");
      }
      return components::Text("on").on_receive(
          ticks, [](const int& value) { received.push_back(value); });
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Element{Host{}});
  EXPECT_EQ(ticks.subscribers(), 1U);
  ticks.set(3);
  ticks.set(4);
  EXPECT_EQ(received, (std::vector<int>{3, 4}));
  toggle->set(false);
  frames(application, 3);
  EXPECT_EQ(ticks.subscribers(), 0U);
  ticks.set(5);
  EXPECT_EQ(received, (std::vector<int>{3, 4}));

  composition::Observable<int> other{1};
  int last = 0;
  const composition::SubscriptionView<composition::Observable<int>> view{
      components::Text("content"), other,
      [&last](const int& value) { last = value; }};
  EXPECT_EQ(&view.publisher(), &other);
  view.action()(9);
  EXPECT_EQ(last, 9);
  EXPECT_EQ(Element{view.content()}.type(),
            composition::ElementType::of<
                composition::PlaceholderContentView::Content>());
}

TEST_F(AppDataParityTest, LazyStateComputesItsValueOncePerInstance) {
  static int made = 0;
  static constexpr composition::LazyState<int> lazy{[] {
    ++made;
    return 12;
  }};
  static constexpr composition::LazyState<std::string> empty{};
  static constexpr composition::StateKey<int> tick{"app-data.tick", 0};
  static std::optional<composition::StateHandle<int>> bump;
  static std::optional<composition::StateHandle<int>> held;
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      bump = context.state(tick);
      held = context.state(lazy);
      const composition::Binding<std::string> projected{context.state(empty)};
      return components::Text(std::to_string(held->get() + bump->get()) +
                              projected.get());
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Element{Host{}});
  EXPECT_EQ(made, 1);
  EXPECT_TRUE(shows(application, "12"));
  bump->set(1);
  frames(application, 3);
  EXPECT_TRUE(shows(application, "13"));
  EXPECT_EQ(made, 1);
  held->set(20);
  frames(application, 3);
  EXPECT_TRUE(shows(application, "21"));
}

TEST_F(AppDataParityTest, TasksTakeANamePriorityAndExecutorPreference) {
  static std::vector<std::string> started;
  static async::ManualExecutor executor;
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      components::Text("tasks")
          .task(composition::TaskOptions{.name = "low",
                                         .priority = async::JobPriority::low},
                []() -> async::Task<void> {
                  started.emplace_back("low");
                  co_return;
                })
          .task(composition::ElementKey{"loading"},
                composition::TaskOptions{.name = "high",
                                         .priority = async::JobPriority::high},
                []() -> async::Task<void> {
                  started.emplace_back("high");
                  co_return;
                })
          .task(composition::TaskOptions{.name = "elsewhere",
                                         .priority = async::JobPriority::normal,
                                         .executor_preference = executor},
                []() -> async::Task<std::expected<void, composition::UiError>> {
                  started.emplace_back("executor");
                  co_return std::expected<void, composition::UiError>{};
                })
          .task(composition::ElementKey{"keyed"},
                composition::TaskOptions{.name = "keyed"},
                []() -> async::Task<std::expected<void, composition::UiError>> {
                  started.emplace_back("keyed");
                  co_return std::expected<void, composition::UiError>{};
                }));
  ASSERT_GE(started.size(), 3U);
  EXPECT_EQ(started.front(), "high");
  EXPECT_EQ(started.back(), "low");
  EXPECT_FALSE(std::ranges::contains(started, std::string{"executor"}));
  executor.drain();
  frames(application, 2);
  EXPECT_TRUE(std::ranges::contains(started, std::string{"executor"}));
  const composition::NodeTask named{
      std::nullopt, composition::NodeTask::Plain{},
      composition::TaskOptions{.name = "named",
                               .priority = async::JobPriority::high}};
  EXPECT_EQ(named.name(), std::optional<std::string>{"named"});
  EXPECT_EQ(named.priority(), async::JobPriority::high);
}

TEST_F(AppDataParityTest, ModifiersComposeThroughEmptyConcatAndTransaction) {
  static int edits = 0;
  const auto concatenated = Tagged{"inner"}.concat(Tagged{"outer"});
  EXPECT_EQ(concatenated.first(), Tagged{"inner"});
  const auto transacted =
      Tagged{"edited"}.transaction([](motion::Transaction& transaction) {
        ++edits;
        transaction.set_disables_animations(true);
      });
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      components::VStack(components::Text("plain")
                             .modifier(composition::EmptyModifier::identity)
                             .id("plain"),
                         components::Text("both").modifier(concatenated),
                         components::Text("edited").modifier(transacted)));
  EXPECT_TRUE(node_of(application, "plain").has_value());
  const auto inner = node_of(application, "inner");
  const auto outer = node_of(application, "outer");
  ASSERT_TRUE(inner.has_value() && outer.has_value());
  const auto inner_node = application.root().tree().node(*inner);
  ASSERT_TRUE(inner_node.has_value());
  ASSERT_TRUE(inner_node->get().parent().has_value());
  EXPECT_TRUE(node_of(application, "edited").has_value());
  EXPECT_EQ(composition::EmptyModifier::identity, composition::EmptyModifier{});
  static_cast<void>(edits);
}

TEST_F(AppDataParityTest, BindingsConvertIndexAndReceiveTheirTransaction) {
  static constexpr composition::StateKey<int> whole{"app-data.whole", 2};
  static const composition::StateKey<std::vector<int>> list{
      "app-data.list", std::vector<int>{1, 2, 3}};
  static std::optional<composition::Binding<int>> number_bound;
  static std::optional<composition::Binding<std::vector<int>>> list_bound;
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      number_bound = composition::Binding<int>{context.state(whole)};
      list_bound = composition::Binding<std::vector<int>>{context.state(list)};
      return components::Text("binding");
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Element{Host{}});
  const composition::Binding<double> widened{*number_bound};
  EXPECT_DOUBLE_EQ(widened.get(), 2.0);
  widened.set(5.0);
  EXPECT_EQ(number_bound->get(), 5);
  const composition::Binding<std::optional<int>> lifted{*number_bound};
  EXPECT_EQ(lifted.get(), std::optional<int>{5});
  lifted.set(std::nullopt);
  EXPECT_EQ(number_bound->get(), 5);
  lifted.set(6);
  EXPECT_EQ(number_bound->get(), 6);
  EXPECT_TRUE(composition::Binding<int>::unwrapping(lifted).has_value());
  const composition::Binding<std::optional<int>> empty =
      composition::Binding<std::optional<int>>::constant(std::nullopt);
  EXPECT_FALSE(composition::Binding<int>::unwrapping(empty).has_value());
  std::vector<std::size_t> indices;
  for (const std::size_t index : list_bound->indices()) {
    indices.push_back(index);
  }
  EXPECT_EQ(indices, (std::vector<std::size_t>{0, 1, 2}));
  (*list_bound)[1].set(20);
  EXPECT_EQ(list_bound->get(), (std::vector<int>{1, 20, 3}));
  bool animated = false;
  int written = 0;
  const composition::Binding<int> transacted{
      [] { return 0; },
      [&animated, &written](int value, const motion::Transaction& transaction) {
        written = value;
        animated = transaction.animation().has_value();
      }};
  motion::with_animation([&transacted] { transacted.set(7); });
  EXPECT_EQ(written, 7);
  EXPECT_TRUE(animated);
}

TEST_F(AppDataParityTest, ForEachReadsBackItsDataAndContent) {
  const components::ForEach each{
      std::vector<int>{4, 5, 6},
      [](int value) { return std::to_string(value); },
      [](int value) { return components::Text(std::to_string(value * 2)); }};
  const auto data = each.data<int>();
  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(data->size(), 3U);
  EXPECT_EQ((*data)[1], 5);
  EXPECT_FALSE(each.data<std::string>().has_value());
  const auto content = each.content<int>();
  ASSERT_TRUE(content.has_value());
  const Element built = (*content)(5);
  EXPECT_EQ(built.type(),
            composition::ElementType::of<components::Text::Content>());
}

TEST_F(AppDataParityTest, ValueTypesListCasesAndMagnitudes) {
  EXPECT_EQ(composition::all_content_modes.size(), 2U);
  EXPECT_EQ(composition::all_content_modes.front(),
            composition::ContentMode::fit);
  EXPECT_EQ(composition::all_visibilities.size(), 3U);
  EXPECT_EQ(composition::all_visibilities.back(),
            composition::Visibility::hidden);
  EXPECT_DOUBLE_EQ(motion::AnimatableTraits<double>::magnitude_squared(3.0),
                   9.0);
  EXPECT_FLOAT_EQ(motion::AnimatableTraits<float>::magnitude_squared(2.0F),
                  4.0F);
  const motion::AnimatableVector velocity{3.0, 4.0};
  EXPECT_DOUBLE_EQ(velocity.magnitude_squared(), 25.0);
  EXPECT_EQ(-velocity, (motion::AnimatableVector{-3.0, -4.0}));
  EXPECT_TRUE((composition::RedactionReasons::invalidated |
               composition::RedactionReasons::placeholder)
                  .contains(composition::RedactionReasons::invalidated));
  EXPECT_FALSE(composition::RedactionReasons::invalidated.contains(
      composition::RedactionReasons::placeholder));
  const theme::Colour linear = theme::ColourMix::rgb(
      theme::RGBColorSpace::srgb_linear, 0.5F, 0.5F, 0.5F, 1.0F);
  EXPECT_NEAR(linear.red, 0.735F, 0.01F);
  const theme::Colour wide = theme::ColourMix::rgb(
      theme::RGBColorSpace::display_p3, 1.0F, 0.0F, 0.0F, 0.5F);
  EXPECT_GT(wide.red, 1.0F);
  EXPECT_FLOAT_EQ(wide.alpha, 0.5F);
}

TEST_F(AppDataParityTest, AnimationStateCarriesKeyedValues) {
  static const motion::AnimationStateKey<int> steps{"app-data.steps", 1};
  motion::AnimationState state;
  EXPECT_EQ(state.size(), 0U);
  EXPECT_EQ(state[steps], 1);
  state.set(steps, 4);
  EXPECT_EQ(state[steps], 4);
  const motion::AnimationContext context;
  const motion::AnimationContext changed = context.with_state(state);
  EXPECT_EQ(context.state(steps), 1);
  EXPECT_EQ(changed.state(steps), 4);
  EXPECT_EQ(changed.animation_state().size(), 1U);
}

TEST_F(AppDataParityTest, TagsCanMatchAnOptionalSelection) {
  const auto plain = composition::TagValue::of(3);
  EXPECT_EQ(plain->get<std::optional<int>>(), nullptr);
  const auto widened = composition::TagValue::of(3, true);
  ASSERT_NE(widened->get<std::optional<int>>(), nullptr);
  EXPECT_EQ(*widened->get<std::optional<int>>(), std::optional<int>{3});
  ASSERT_NE(widened->get<int>(), nullptr);
  const Element tagged = components::Text("row").tag(4, true);
  ASSERT_NE(tagged.tag(), nullptr);
  EXPECT_EQ(*tagged.tag()->get<std::optional<int>>(), std::optional<int>{4});
}

TEST_F(AppDataParityTest, AnchorPreferencesTransformAndCollectArrays) {
  static const composition::PreferenceKey<std::vector<composition::Rect>>
      frames_key{"app-data.frames",
                 {},
                 [](std::vector<composition::Rect>& value,
                    std::vector<composition::Rect> next) {
                   value.insert(value.end(), next.begin(), next.end());
                 }};
  static std::vector<composition::Rect> collected;
  provider::headless::HeadlessProviderFactory providers;
  using Source = composition::AnchorSource<composition::Rect>;
  Application application = start(
      providers,
      components::VStack(
          components::ZStack()
              .frame({.width = 50.0F, .height = 20.0F})
              .anchor_preference(
                  frames_key, Source::bounds(),
                  [](const composition::Anchor<composition::Rect>&) {
                    return std::vector<composition::Rect>{
                        composition::Rect{.width = 1.0F}};
                  })
              .transform_anchor_preference(
                  frames_key,
                  Source::rect(
                      composition::Rect{.width = 5.0F, .height = 5.0F}),
                  [](std::vector<composition::Rect>& value,
                     const composition::Anchor<composition::Rect>&) {
                    value.push_back(composition::Rect{.width = 2.0F});
                  }))
          .on_preference_change(
              frames_key, [](const std::vector<composition::Rect>& value) {
                collected = value;
              }));
  ASSERT_EQ(collected.size(), 2U);
  EXPECT_FLOAT_EQ(collected[1].width, 2.0F);
  const composition::AnchorSource<std::vector<composition::Rect>> both{
      std::vector<Source>{Source::bounds(),
                          Source::rect(composition::Rect{
                              .x = 1.0F, .width = 2.0F, .height = 2.0F})}};
  const std::vector<composition::Rect> resolved =
      both.resolve(composition::Rect{
          .x = 10.0F, .y = 0.0F, .width = 20.0F, .height = 10.0F});
  ASSERT_EQ(resolved.size(), 2U);
  EXPECT_FLOAT_EQ(resolved[0].width, 20.0F);
  EXPECT_FLOAT_EQ(resolved[1].x, 11.0F);
  EXPECT_EQ(both.sources().size(), 2U);
}

TEST_F(AppDataParityTest, OpenURLHandlersRedirectTheSystemAction) {
  const auto redirected = provider::Url::create("https://example.com/other");
  ASSERT_TRUE(redirected.has_value());
  provider::headless::HeadlessProviderFactory providers;
  const components::OpenURLAction system =
      components::OpenURLAction::system(providers.browser());
  const components::OpenURLAction handled = components::OpenURLAction::merged(
      system, components::OpenURLAction{[&redirected](const provider::Url&) {
        return components::OpenURLAction::Result::system_action(*redirected,
                                                                true);
      }});
  EXPECT_TRUE(handled("https://example.com/first"));
  ASSERT_EQ(providers.browser().opened().size(), 1U);
  EXPECT_EQ(providers.browser().opened().front(), *redirected);
  const components::OpenURLAction::Result plain =
      components::OpenURLAction::Result::system_action;
  EXPECT_EQ(plain.kind(),
            components::OpenURLAction::Result::Kind::system_action);
  EXPECT_FALSE(plain.url().has_value());
  const components::OpenURLAction::Result moved =
      components::OpenURLAction::Result::system_action(*redirected);
  EXPECT_FALSE(moved.prefers_in_app());
  EXPECT_EQ(moved.url(), redirected);
}

TEST_F(AppDataParityTest, ClaimedAndLaunchURLsReachOnOpenURL) {
  static std::vector<std::string> opened;
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const components::OpenURLAction& open =
          context.environment(components::Environment::open_url);
      return components::Button(
                 "open",
                 [open] {
                   static_cast<void>(open("https://arisa.app/notes/2"));
                 })
          .id("open")
          .on_open_url([](const provider::Url& url) {
            opened.emplace_back(url.value());
          });
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      with_files(providers, Element{Host{}}, std::nullopt,
                 LinkOptions{.claimed = {"https://arisa.app/"},
                             .launch_url = "https://arisa.app/notes/1"});
  frames(application, 2);
  ASSERT_EQ(opened.size(), 1U);
  EXPECT_EQ(opened.front(), "https://arisa.app/notes/1");
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"open"}));
  frames(application, 2);
  ASSERT_EQ(opened.size(), 2U);
  EXPECT_EQ(opened.back(), "https://arisa.app/notes/2");
  EXPECT_TRUE(providers.browser().opened().empty());
  application.open_url(*provider::Url::create("https://arisa.app/x"));
  EXPECT_EQ(opened.size(), 3U);
}

TEST_F(AppDataParityTest, BackgroundTasksRunWhenTheSceneLeaves) {
  static std::vector<std::string> ran;
  static std::optional<composition::BackgroundTaskScheduler> scheduler;
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      scheduler =
          context.environment(composition::BackgroundTaskScheduler::key);
      using composition::BackgroundTask;
      return components::Text("tasks")
          .background_task(BackgroundTask::app_refresh("refresh"),
                           [](const BackgroundTask::Request& request) {
                             ran.push_back("refresh:" + request.identifier);
                           })
          .background_task(BackgroundTask::processing_task("index"),
                           [](const BackgroundTask::Request& request) {
                             ran.push_back("index:" + request.identifier);
                           })
          .background_task(BackgroundTask::url_session,
                           [](const BackgroundTask::Request& request) {
                             ran.push_back("session:" + request.identifier);
                           })
          .background_task(BackgroundTask::url_session("named"),
                           [](const BackgroundTask::Request& request) {
                             ran.push_back("named:" + request.identifier);
                           })
          .background_task(
              BackgroundTask::url_session([](std::string_view identifier) {
                return identifier.starts_with("dl.");
              }),
              [](const BackgroundTask::Request& request) {
                ran.push_back("matching:" + request.identifier);
              });
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Element{Host{}});
  ASSERT_TRUE(scheduler.has_value());
  using Kind = composition::BackgroundTask::Kind;
  scheduler->submit({.kind = Kind::app_refresh, .identifier = "refresh"});
  scheduler->submit({.kind = Kind::processing_task, .identifier = "index"});
  scheduler->submit({.kind = Kind::url_session, .identifier = "dl.1"});
  scheduler->submit({.kind = Kind::app_refresh, .identifier = "other"});
  EXPECT_EQ(scheduler->pending(), 4U);
  frames(application, 2);
  EXPECT_TRUE(ran.empty());
  providers.lifecycle().set_phase(provider::ScenePhase::background);
  frames(application, 2);
  EXPECT_EQ(scheduler->pending(), 0U);
  EXPECT_TRUE(std::ranges::contains(ran, std::string{"refresh:refresh"}));
  EXPECT_TRUE(std::ranges::contains(ran, std::string{"index:index"}));
  EXPECT_TRUE(std::ranges::contains(ran, std::string{"session:dl.1"}));
  EXPECT_TRUE(std::ranges::contains(ran, std::string{"matching:dl.1"}));
  EXPECT_FALSE(std::ranges::contains(ran, std::string{"named:dl.1"}));
  EXPECT_FALSE(std::ranges::contains(ran, std::string{"refresh:other"}));
}

TEST_F(AppDataParityTest, SystemFlagsFollowTheConsole) {
  static bool captured = true;
  static bool authenticates = true;
  static bool reduced = false;
  static bool undo_present = false;
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      captured =
          context.environment(components::Environment::is_scene_captured);
      authenticates = context.environment(
          components::Environment::is_user_authentication_enabled);
      reduced = context.environment(
          components::Environment::system_prefers_reduced_resource_usage);
      undo_present = context.environment(components::Environment::undo_manager)
                         .has_value();
      return components::Text("flags");
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  providers.system_info().set_battery(provider::BatteryState{
      .percent = provider::BatteryPercent::of<10>(), .charging = false});
  Application application = start(providers, Element{Host{}});
  frames(application, 2);
  EXPECT_FALSE(captured);
  EXPECT_FALSE(authenticates);
  EXPECT_TRUE(reduced);
  EXPECT_TRUE(undo_present);
}

TEST_F(AppDataParityTest, UndoManagerUndoesRedoesAndGroups) {
  const composition::UndoManager undo;
  int value = 0;
  const auto set = [&value, &undo](int next, auto& self) -> void {
    const int previous = value;
    value = next;
    undo.register_undo("Set", [previous, &self, &undo_value = value,
                               &undo](const composition::UndoManager&) {
      static_cast<void>(undo_value);
      static_cast<void>(undo);
      self(previous, self);
    });
  };
  set(1, set);
  set(2, set);
  EXPECT_TRUE(undo.can_undo());
  EXPECT_EQ(undo.undo_action_name(), "Set");
  undo.undo();
  EXPECT_EQ(value, 1);
  EXPECT_TRUE(undo.can_redo());
  undo.redo();
  EXPECT_EQ(value, 2);
  undo.begin_undo_grouping();
  set(3, set);
  set(4, set);
  undo.end_undo_grouping();
  undo.set_action_name("Both");
  EXPECT_EQ(undo.undo_action_name(), "Both");
  undo.undo();
  EXPECT_EQ(value, 2);
  EXPECT_EQ(undo.redo_action_name(), "Both");
  EXPECT_EQ(undo.status().get().can_redo, true);
  undo.set_levels_of_undo(1);
  EXPECT_EQ(undo.levels_of_undo(), 1U);
  undo.remove_all_actions();
  EXPECT_FALSE(undo.can_undo());
  EXPECT_FALSE(undo.can_redo());
}

TEST_F(AppDataParityTest, HiddenSystemOverlaysEmptyTheHintBar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            components::VStack(components::Button("go", [] {}).id("go"),
                               Element{components::HintBar{}}.id("hints"))
                .persistent_system_overlays(composition::Visibility::hidden));
  frames(application, 3);
  const auto hints = node_named(application, "hints");
  ASSERT_TRUE(hints.has_value());
  EXPECT_EQ(application.root().tree().root().preference(
                composition::PersistentSystemOverlays::key),
            std::optional{composition::Visibility::hidden});
  std::size_t texts_in_bar = 0;
  for (const composition::Node& child : hints->get().children()) {
    texts_in_bar += child.children().size();
  }
  EXPECT_EQ(texts_in_bar, 0U);
}

TEST_F(AppDataParityTest, PageIndexStylesPickTheirBackground) {
  using Mode = structure::PageIndexViewStyle::BackgroundDisplayMode;
  EXPECT_EQ(structure::PageIndexViewStyle::all_background_modes.size(), 4U);
  EXPECT_EQ(structure::PageIndexViewStyle{}.background_display_mode(),
            Mode::automatic);
  EXPECT_TRUE(structure::IndexViewStyle::page(Mode::always).backs(false));
  EXPECT_FALSE(structure::IndexViewStyle::page(Mode::never).backs(true));
  EXPECT_TRUE(structure::IndexViewStyle::page(Mode::interactive).backs(true));
  EXPECT_FALSE(structure::IndexViewStyle::page().backs(false));
  EXPECT_EQ(
      structure::IndexViewStyle{structure::PageIndexViewStyle{Mode::never}}
          .background_display_mode(),
      Mode::never);
  static std::optional<structure::IndexViewStyle> seen;
  class Reader final : public composition::Component {
   public:
    auto operator==(const Reader&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      seen = context.environment(structure::IndexViewStyle::key);
      return components::Text("style");
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, Element{Reader{}}.index_view_style(
                           structure::IndexViewStyle::page(Mode::always)));
  ASSERT_TRUE(seen.has_value());
  EXPECT_EQ(seen->background_display_mode(), Mode::always);
}

TEST_F(AppDataParityTest, UnavailableViewsTakeAnImageResource) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, components::ContentUnavailableView(
                           "Empty", components::Image::Resource{"inbox"},
                           components::Text("Nothing here")));
  EXPECT_TRUE(shows(application, "Empty"));
  EXPECT_TRUE(shows(application, "Nothing here"));
}

TEST_F(AppDataParityTest, AssistiveAccessScenesReplaceTheStandardContent) {
  provider::headless::HeadlessProviderFactory providers;
  Application standard =
      start(providers, components::Text("standard")
                           .assistive_access(structure::AssistiveAccess{[] {
                             return Element{components::Text("simple")};
                           }}));
  EXPECT_TRUE(shows(standard, "standard"));
  EXPECT_FALSE(shows(standard, "simple"));
  provider::headless::HeadlessProviderFactory assisted;
  Application simple = start(
      assisted,
      components::ZStack(components::Text("standard")
                             .assistive_access(structure::AssistiveAccess{[] {
                               return Element{components::Text("simple")};
                             }}))
          .environment(
              components::Environment::accessibility_assistive_access_enabled,
              true));
  EXPECT_TRUE(shows(simple, "simple"));
  EXPECT_TRUE(
      node_of(simple, structure::AssistiveAccess::scene_key().description())
          .has_value());
}

TEST_F(AppDataParityTest, BenchmarkHostDrivesFramesAndFindsViews) {
  static constexpr composition::StateKey<int> counter{"app-data.counter", 6};
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto state = context.state(counter);
      return components::Button(std::to_string(state.get()),
                                [state] { state.set(state.get() + 1); })
          .id("counter");
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Element{Host{}});
  BenchmarkHost host{application};
  const auto one = host.measure_render(BenchmarkHost::nominal_frame);
  ASSERT_TRUE(one.has_value());
  EXPECT_EQ(one->frames, 1U);
  const auto many = host.measure_renders(std::chrono::milliseconds{50});
  ASSERT_TRUE(many.has_value());
  EXPECT_EQ(many->frames, 3U);
  const auto seconds = host.measure_renders(0.1);
  ASSERT_TRUE(seconds.has_value());
  EXPECT_EQ(seconds->frames, 6U);
  const auto acted = host.measure_action([&application] {
    static_cast<void>(
        application.root().activate(composition::ElementKey{"counter"}));
  });
  ASSERT_TRUE(acted.has_value());
  EXPECT_GE(acted->rendered, 1U);
  EXPECT_EQ(
      host.state_for_identifier(composition::ElementKey{"counter"}, counter),
      std::nullopt);
  const auto view =
      host.view_for_identifier(composition::ElementKey{"counter"});
  ASSERT_TRUE(view.has_value());
  EXPECT_FALSE(
      host.view_for_identifier(composition::ElementKey{"missing"}).has_value());
  EXPECT_GE(acted->mean().count(), 0);
}

TEST_F(AppDataParityTest, ViewDebugSerialisesTheTree) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, components::Text("debug").id("debugged"));
  const std::vector<std::byte> bytes =
      application.introspection().serialized_data();
  std::string text(bytes.size(), '\0');
  std::ranges::transform(bytes, text.begin(), [](std::byte value) {
    return static_cast<char>(value);
  });
  EXPECT_TRUE(text.starts_with("{"));
  EXPECT_NE(text.find("\"nodes\""), std::string::npos);
  EXPECT_NE(text.find("debugged"), std::string::npos);
}

TEST_F(AppDataParityTest, ExistingStyleOverloadsReachTheNode) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      components::VStack(
          components::Text("bordered")
              .border(graphics::ShapeStyle{theme::ColourValue{
                          theme::ColourRole::accent}},
                      2.0F)
              .id("bordered"),
          components::Text("overlaid")
              .overlay(
                  graphics::ShapeStyle{
                      theme::ColourValue{theme::ColourRole::accent}},
                  graphics::AnyShape{theme::Rectangle()}, graphics::FillStyle{})
              .id("overlaid"),
          components::Text("edge")
              .overlay(graphics::ShapeStyle{theme::ColourValue{
                           theme::ColourRole::accent}},
                       theme::Edge::all)
              .id("edge"),
          components::Text("range")
              .allowed_dynamic_range(composition::ImageDynamicRange::standard)
              .id("range")));
  EXPECT_TRUE(node_of(application, "bordered").has_value());
  EXPECT_TRUE(node_of(application, "overlaid").has_value());
  EXPECT_TRUE(node_of(application, "edge").has_value());
  EXPECT_TRUE(node_of(application, "range").has_value());
}

}
