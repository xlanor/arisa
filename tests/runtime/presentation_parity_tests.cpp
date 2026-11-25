#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/file_url.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/presentation_placement.hpp>
#include <arisa/composition/presentation_sizing.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/user_defaults.hpp>
#include <arisa/composition/ut_type.hpp>
#include <arisa/presentation/alert.hpp>
#include <arisa/presentation/presentation_layer.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/memory_storage.hpp>
#include <arisa/structure/file_browser.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Element;
using composition::PresentationSizing;

class PresentationParityTest : public ScrollFixture {
 protected:
  class Host final : public composition::Component {
   public:
    using Build = std::function<Element(composition::Context&)>;

    explicit Host(std::shared_ptr<const Build> build)
        : build_(std::move(build)) {}

    auto operator==(const Host& other) const -> bool {
      return build_ == other.build_;
    }

    auto body(composition::Context& context) const -> Element override {
      return (*build_)(context);
    }

   private:
    std::shared_ptr<const Build> build_;
  };

  class Failure {
   public:
    explicit Failure(int code) noexcept : code_(code) {}

    auto error_description() const -> std::string {
      return std::format("Failed {}", code_);
    }

    auto code() const noexcept -> int { return code_; }

    friend auto operator==(const Failure&, const Failure&) -> bool = default;

   private:
    int code_;
  };

  static constexpr composition::StateKey<bool> flag{"parity-flag", false};

  static auto host(Host::Build build) -> Element {
    return Host{std::make_shared<const Host::Build>(std::move(build))};
  }

  static auto body(std::string key) -> Element {
    return components::ZStack()
        .frame({.max_width = composition::infinity,
                .max_height = composition::infinity})
        .id(std::move(key));
  }

  static auto fixed(std::string key, float width, float height) -> Element {
    return components::ZStack()
        .frame({.width = width, .height = height})
        .id(std::move(key));
  }

  static void present(provider::headless::HeadlessProviderFactory& providers,
                      Application& application) {
    frames(application, 2);
    for (int index = 0; index < 40; ++index) {
      step(providers, application, 32ms);
    }
  }

  static void activate(Application& application, std::string_view key) {
    EXPECT_TRUE(application.root().activate(composition::ElementKey{key}));
  }

  static auto button(Application& application, std::string_view label)
      -> std::optional<composition::NodeId> {
    std::optional<composition::NodeId> found;
    const auto search = [&](this const auto& self,
                            const composition::Node& node) -> void {
      if (found.has_value() ||
          node.phase() == composition::NodeLifecyclePhase::departing) {
        return;
      }
      if (const auto pressed = node.content_as<components::Button::Content>();
          pressed.has_value() && pressed->get().label() == label) {
        found = node.id();
        return;
      }
      for (const composition::Node& child : node.children()) {
        self(child);
      }
    };
    search(application.root().tree().root());
    return found;
  }

  static auto sheet_with(std::function<Element()> content) -> Element {
    return host([content = std::move(content)](composition::Context& context) {
      const auto shown = context.state(flag);
      return components::Button("open", [shown] { shown.set(true); })
          .id("open")
          .sheet(shown, content);
    });
  }

  static auto opened(provider::headless::HeadlessProviderFactory& providers,
                     std::function<Element()> content) -> Application {
    Application application = start(providers, sheet_with(std::move(content)));
    EXPECT_TRUE(application.root().activate(composition::ElementKey{"open"}));
    present(providers, application);
    return application;
  }
};

TEST_F(PresentationParityTest, PlacementAlignsTheSheet) {
  const auto placed = [](composition::PresentationPlacement placement) {
    provider::headless::HeadlessProviderFactory providers;
    Application application =
        start(providers, host([placement](composition::Context& context) {
                const auto shown = context.state(flag);
                return components::Button("open", [shown] { shown.set(true); })
                    .id("open")
                    .sheet(shown, [placement] {
                      return body("panel")
                          .presentation_sizing(PresentationSizing::form)
                          .presentation_placement(placement);
                    });
              }));
    activate(application, "open");
    present(providers, application);
    return presented_of(application, "panel");
  };
  const composition::Rect centre =
      placed(composition::PresentationPlacement::center);
  const composition::Rect leading =
      placed(composition::PresentationPlacement::leading);
  const composition::Rect trailing =
      placed(composition::PresentationPlacement::trailing);
  const composition::Rect automatic =
      placed(composition::PresentationPlacement::automatic);
  EXPECT_LT(leading.x, centre.x - 100.0F);
  EXPECT_GT(trailing.x, centre.x + 100.0F);
  EXPECT_NEAR(automatic.x, centre.x, 1.0F);
  EXPECT_NEAR(centre.x + (centre.width * 0.5F), 640.0F, 2.0F);
}

TEST_F(PresentationParityTest, SizingKindsProposeTheirSizes) {
  const composition::PresentationSizingContext context{
      composition::Size{.width = 1000.0F, .height = 800.0F}};
  const composition::PresentationSizingRoot root{
      [](composition::ProposedViewSize) {
        return composition::Size{.width = 300.0F, .height = 200.0F};
      }};
  const composition::ProposedViewSize page =
      PresentationSizing::page.proposed_size(root, context);
  EXPECT_NEAR(page.width.value_or(0.0F), 620.0F, 0.5F);
  EXPECT_NEAR(page.height.value_or(0.0F), 704.0F, 0.5F);
  const composition::ProposedViewSize form =
      PresentationSizing::form.proposed_size(root, context);
  EXPECT_NEAR(form.width.value_or(0.0F), 500.0F, 0.5F);
  const composition::ProposedViewSize fitted =
      PresentationSizing::fitted.proposed_size(root, context);
  EXPECT_FALSE(fitted.width.has_value());
  EXPECT_FALSE(fitted.height.has_value());
  const composition::ProposedViewSize automatic =
      PresentationSizing::automatic.proposed_size(root, context);
  EXPECT_TRUE(automatic.width.has_value());
  const composition::ProposedViewSize partly =
      PresentationSizing::form.fitting(false, true)
          .proposed_size(root, context);
  EXPECT_NEAR(partly.width.value_or(0.0F), 500.0F, 0.5F);
  EXPECT_FALSE(partly.height.has_value());
  EXPECT_FALSE(PresentationSizing::form.fitting(false, true).plain());
  EXPECT_TRUE(PresentationSizing::form.sticky(true, false).sticky_width());
  EXPECT_FALSE(PresentationSizing::form.sticky().sticky_height());
}

TEST_F(PresentationParityTest, CustomSizingConformanceSizesTheSheet) {
  class Quarter {
   public:
    auto proposed_size(const composition::PresentationSizingRoot&,
                       const composition::PresentationSizingContext& context)
        const -> composition::ProposedViewSize {
      return composition::ProposedViewSize{context.container().width * 0.25F,
                                           context.container().height * 0.5F};
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application = opened(providers, [] {
    return body("panel").presentation_sizing(PresentationSizing{Quarter{}});
  });
  const composition::Rect panel = presented_of(application, "panel");
  EXPECT_NEAR(panel.width, 320.0F - 48.0F, 4.0F);
  EXPECT_LT(panel.height, 400.0F);
  EXPECT_EQ(PresentationSizing{Quarter{}}.kind(),
            PresentationSizing::Kind::custom);
}

TEST_F(PresentationParityTest, FittedSizingHugsTheContentOnTheChosenAxis) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = opened(providers, [] {
    return fixed("panel", 240.0F, 160.0F)
        .presentation_sizing(PresentationSizing::page.fitting(true, true));
  });
  const composition::Rect panel = presented_of(application, "panel");
  EXPECT_NEAR(panel.width, 240.0F, 1.0F);
  EXPECT_NEAR(panel.height, 160.0F, 1.0F);
  provider::headless::HeadlessProviderFactory sticky_providers;
  Application sticky = opened(sticky_providers, [] {
    return body("panel").presentation_sizing(
        PresentationSizing::form.sticky(true, true));
  });
  EXPECT_GT(presented_of(sticky, "panel").width, 200.0F);
}

TEST_F(PresentationParityTest, CompactAdaptationFollowsEachAxis) {
  const auto anchored = [](components::UserInterfaceSizeClass horizontal,
                           components::UserInterfaceSizeClass vertical) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(
        providers, host([horizontal, vertical](composition::Context& context) {
          const auto shown = context.state(flag);
          return Element{
              components::ZStack(
                  components::ZStackOptions{
                      .alignment = composition::Alignment::top_leading},
                  components::Button("anchor", [shown] { shown.set(true); })
                      .id("anchor")
                      .popover(
                          shown,
                          [] {
                            return fixed("panel", 200.0F, 80.0F)
                                .presentation_compact_adaptation(
                                    composition::PresentationAdaptation::
                                        popover,
                                    composition::PresentationAdaptation::sheet);
                          }))
                  .frame({.max_width = composition::infinity,
                          .max_height = composition::infinity})}
              .environment(components::Environment::horizontal_size_class,
                           horizontal)
              .environment(components::Environment::vertical_size_class,
                           vertical);
        }));
    activate(application, "anchor");
    present(providers, application);
    const composition::Rect panel = presented_of(application, "panel");
    const composition::Rect anchor = presented_of(application, "anchor");
    return std::abs((panel.x + (panel.width * 0.5F)) -
                    (anchor.x + (anchor.width * 0.5F))) < 40.0F;
  };
  EXPECT_TRUE(anchored(components::UserInterfaceSizeClass::compact,
                       components::UserInterfaceSizeClass::regular));
  EXPECT_FALSE(anchored(components::UserInterfaceSizeClass::regular,
                        components::UserInterfaceSizeClass::compact));
}

TEST_F(PresentationParityTest, BackgroundViewSitsBehindTheSheetContent) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = opened(providers, [] {
    return body("panel").presentation_background(
        composition::Alignment::bottom_trailing, [] {
          return Element{components::ZStack(fixed("backdrop", 40.0F, 40.0F))};
        });
  });
  ASSERT_TRUE(node_of(application, "backdrop").has_value());
  const composition::Rect panel = presented_of(application, "panel");
  const composition::Rect backdrop = presented_of(application, "backdrop");
  EXPECT_GT(backdrop.x, panel.x + (panel.width * 0.5F));
  EXPECT_GT(backdrop.y, panel.y + (panel.height * 0.5F));
  provider::headless::HeadlessProviderFactory other;
  Application centred = opened(other, [] {
    return body("panel").presentation_background([] {
      return Element{components::ZStack(fixed("backdrop", 40.0F, 40.0F))};
    });
  });
  const composition::Rect middle = presented_of(centred, "panel");
  const composition::Rect behind = presented_of(centred, "backdrop");
  EXPECT_NEAR(behind.x + 20.0F, middle.x + (middle.width * 0.5F), 2.0F);
}

TEST_F(PresentationParityTest, ErrorItemAlertTitlesFromTheError) {
  static constexpr composition::StateKey<std::optional<int>> failing{
      "parity-failing", std::nullopt};
  static std::optional<composition::StateHandle<std::optional<int>>> handle;
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, host([](composition::Context& context) {
        handle = context.state(failing);
        const composition::Binding<std::optional<Failure>> error{
            [shown = *handle] {
              return shown.get().has_value()
                         ? std::optional<Failure>{Failure{*shown.get()}}
                         : std::nullopt;
            },
            [shown = *handle](std::optional<Failure> value) {
              shown.set(value.has_value() ? std::optional<int>{value->code()}
                                          : std::nullopt);
            }};
        return components::VStack(
            components::Text("first").alert(
                error,
                [](const Failure& failure) {
                  return components::Button(
                      std::format("Retry {}", failure.code()), [] {});
                }),
            components::Text("second").alert(
                error, [] { return components::Button("Dismiss", [] {}); },
                [](const Failure& failure) {
                  return components::Text(
                      std::format("Code {}", failure.code()));
                }));
      }));
  handle->set(7);
  present(providers, application);
  EXPECT_TRUE(shows(application, "Failed 7"));
  EXPECT_TRUE(button(application, "Retry 7").has_value() ||
              shows(application, "Code 7"));
  press(providers, application, provider::Button::b);
  present(providers, application);
  EXPECT_FALSE(handle->get().has_value());
}

TEST_F(PresentationParityTest, ConfirmationDialogPresentingAndItem) {
  static constexpr composition::StateKey<std::optional<int>> chosen{
      "parity-chosen", std::nullopt};
  static std::optional<composition::StateHandle<std::optional<int>>> item;
  static std::optional<composition::StateHandle<bool>> shown;
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context& context) {
              item = context.state(chosen);
              shown = context.state(flag);
              return components::VStack(
                  components::Text("one").confirmation_dialog(
                      "Delete?", composition::Binding<bool>{*shown},
                      composition::Visibility::visible, std::optional<int>{3},
                      [](const int& value) {
                        return components::Button(
                            std::format("Delete {}", value),
                            components::ButtonRole::destructive, [] {});
                      }),
                  components::Text("two").confirmation_dialog(
                      "Move?", *item, composition::Visibility::visible,
                      [](const int& value) {
                        return components::Button(std::format("Move {}", value),
                                                  [] {});
                      }));
            }));
  shown->set(true);
  present(providers, application);
  EXPECT_TRUE(button(application, "Delete 3").has_value());
  EXPECT_TRUE(shows(application, "Delete?"));
  press(providers, application, provider::Button::b);
  present(providers, application);
  EXPECT_FALSE(shown->get());
  item->set(5);
  present(providers, application);
  EXPECT_TRUE(button(application, "Move 5").has_value() ||
              shows(application, "Move 5"));
  press(providers, application, provider::Button::b);
  present(providers, application);
  EXPECT_FALSE(item->get().has_value());
}

TEST_F(PresentationParityTest, LegacyAlertAndActionSheetBuildDialogs) {
  static std::optional<composition::StateHandle<bool>> shown;
  static int pressed = 0;
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context& context) {
              shown = context.state(flag);
              return components::Text("presenter")
                  .alert(composition::Binding<bool>{*shown}, [] {
                    return presentation::Alert(
                        "Sign out?",
                        std::optional<std::string>{"You can sign in later"},
                        presentation::Alert::Button::destructive(
                            "Sign out", [] { ++pressed; }),
                        presentation::Alert::Button::cancel());
                  });
            }));
  shown->set(true);
  present(providers, application);

  EXPECT_TRUE(shows(application, "Sign out?"));
  EXPECT_TRUE(shows(application, "You can sign in later"));
  EXPECT_TRUE(button(application, "Cancel").has_value());
  const auto sign_out = button(application, "Sign out");
  ASSERT_TRUE(sign_out.has_value());
  ASSERT_TRUE(application.root().activate(*sign_out));
  present(providers, application);
  EXPECT_EQ(pressed, 1);
  const presentation::Alert single{"Saved"};
  ASSERT_EQ(single.buttons().size(), 1U);
  EXPECT_EQ(single.buttons()[0].label(),
            presentation::PresentationLayer::ok_label);
  provider::headless::HeadlessProviderFactory sheet_providers;
  static std::optional<composition::StateHandle<bool>> sheet_shown;
  Application sheet = start(
      sheet_providers, host([](composition::Context& context) {
        sheet_shown = context.state(flag);
        return components::Text("presenter")
            .action_sheet(composition::Binding<bool>{*sheet_shown}, [] {
              return presentation::ActionSheet(
                  "Share", std::nullopt,
                  {presentation::Alert::Button::default_button("Copy link"),
                   presentation::Alert::Button::cancel()});
            });
      }));
  sheet_shown->set(true);
  present(sheet_providers, sheet);
  EXPECT_TRUE(shows(sheet, "Share"));
  EXPECT_TRUE(button(sheet, "Copy link").has_value() ||
              shows(sheet, "Copy link"));
}

TEST_F(PresentationParityTest, DialogExtrasFromThePresenterReachTheAlert) {
  static std::optional<composition::StateHandle<bool>> shown;
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, host([](composition::Context& context) {
        shown = context.state(flag);
        return components::Text("presenter")
            .alert("Delete?", composition::Binding<bool>{*shown},
                   [] {
                     return components::Button(
                         "Delete", components::ButtonRole::destructive, [] {});
                   })
            .dialog_icon(
                components::Image::system_name("delete").id("dialog-icon"))
            .dialog_severity(composition::DialogSeverity::critical);
      }));
  shown->set(true);
  present(providers, application);
  EXPECT_TRUE(shows(application, "Delete?"));
  EXPECT_TRUE(node_of(application, "dialog-icon").has_value());
}

TEST_F(PresentationParityTest, FileDialogModifiersShapeTheBrowser) {
  static constexpr composition::StateKey<bool> open{"parity-files-open", true};
  class Files final : public composition::Component {
   public:
    auto operator==(const Files&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      return components::Text("files")
          .file_importer(composition::Binding<bool>{context.state(open)},
                         {composition::UTType::plain_text()},
                         [](structure::FileBrowser::Result) {})
          .file_dialog_confirmation_label("Attach")
          .file_dialog_message("Pick a note")
          .file_dialog_default_directory(
              composition::FileURL::create("switch/notes"))
          .file_dialog_customization_id("notes")
          .file_dialog_url_enabled([](const composition::FileURL& url) {
            return url.last_path_component() != "locked.txt";
          });
    }
  };
  provider::headless::MemoryStorage card;
  card.add(*provider::AssetPath::create("switch/notes/readme.txt"), "hello");
  card.add(*provider::AssetPath::create("switch/notes/locked.txt"), "no");
  card.add(*provider::AssetPath::create("atmosphere/config.ini"), "y");
  provider::headless::HeadlessProviderFactory providers;
  auto created =
      Application::create(providers, Files{},
                          ApplicationOptions{.executors = std::nullopt,
                                             .fonts = {},
                                             .atlas = {},
                                             .interaction = {},
                                             .files = card});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  frames(application, 40);
  EXPECT_TRUE(shows(application, "Attach"));
  EXPECT_TRUE(shows(application, "Pick a note"));
  ASSERT_TRUE(
      node_of(application,
              structure::FileBrowser::row_key("readme.txt").description())
          .has_value());
  const auto locked = node_named(
      application, structure::FileBrowser::row_key("locked.txt").description());
  ASSERT_TRUE(locked.has_value());
  EXPECT_TRUE(locked->get().interactions().disabled);
}

TEST_F(PresentationParityTest, FileDialogCustomizationRemembersTheFolder) {
  static constexpr composition::StateKey<bool> open{"parity-files-remember",
                                                    true};
  class Files final : public composition::Component {
   public:
    auto operator==(const Files&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      return components::Text("files")
          .file_importer(composition::Binding<bool>{context.state(open)},
                         {composition::UTType::plain_text()},
                         [](structure::FileBrowser::Result) {})
          .file_dialog_customization_id("notes");
    }
  };
  provider::headless::MemoryStorage card;
  card.add(*provider::AssetPath::create("switch/notes/readme.txt"), "hello");
  card.add(*provider::AssetPath::create("atmosphere/config.ini"), "y");
  const composition::UserDefaults store;
  provider::headless::HeadlessProviderFactory providers;
  auto created = Application::create(
      providers, Element{Files{}}.default_app_storage(store),
      ApplicationOptions{.executors = std::nullopt,
                         .fonts = {},
                         .atlas = {},
                         .interaction = {},
                         .files = card});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  frames(application, 30);
  ASSERT_TRUE(
      application.root().activate(structure::FileBrowser::row_key("switch")));
  frames(application, 30);
  EXPECT_EQ(store.get<std::string>("arisa.file-dialog.notes"),
            std::optional<std::string>{"switch"});
  EXPECT_TRUE(node_of(application,
                      structure::FileBrowser::row_key("notes").description())
                  .has_value());
}

}
