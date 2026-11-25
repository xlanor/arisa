#include <arisa/components/glass_panel.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/presentation/help.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/callout_board.hpp>
#include <arisa/structure/dock_bar.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/metrics.hpp>

#include <gallery_fixture.hpp>
#include <panels_page.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::runtime {

class GalleryPanelsTest : public GalleryFixture {
 protected:
  using Page = examples::PanelsPage;

  static auto open(provider::headless::HeadlessProviderFactory& providers)
      -> Application {
    Application application =
        start_gallery(providers, examples::GalleryPage::panels);
    run(providers, application, 10);
    return application;
  }

  static auto rendered(provider::headless::HeadlessProviderFactory& providers,
                       theme::ColorScheme scheme, bool docked) -> Application {
    const provider::SurfaceSize size =
        docked ? provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                                       .height = provider::SurfaceHeight{1080}}
               : provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                                       .height = provider::SurfaceHeight{720}};
    providers.surface().set_display_mode(docked
                                             ? provider::DisplayMode::docked
                                             : provider::DisplayMode::handheld);
    providers.gpu().rasterize(size);
    Application application =
        start_gallery(providers, examples::GalleryPage::panels, scheme, size);
    run(providers, application, 120);
    return application;
  }

  static auto reference(std::string_view key) -> double {
    std::ifstream stream{ARISA_SWIFTUI_REFERENCE};
    std::string line;
    while (std::getline(stream, line)) {
      std::istringstream fields{line};
      std::string name;
      std::string light;
      std::getline(fields, name, '\t');
      std::getline(fields, light, '\t');
      if (name == key) {
        return std::stod(light);
      }
    }
    ADD_FAILURE() << key << " is not in the SwiftUI reference";
    return 0.0;
  }

  static auto keyed(Application& application, std::string_view key)
      -> const composition::Node* {
    const auto id =
        application.root().tree().find(composition::ElementKey{key});
    if (!id.has_value()) {
      ADD_FAILURE() << "no node keyed " << key;
      return nullptr;
    }
    return &application.root().tree().node(*id)->get();
  }

  static auto inside(const composition::Node& outer, std::string_view key)
      -> const composition::Node* {
    const composition::Node* found = nullptr;
    walk(outer, [&](const composition::Node& node) {
      if (found == nullptr && node.key() == composition::ElementKey{key} &&
          node.phase() == composition::NodeLifecyclePhase::live) {
        found = &node;
      }
    });
    return found;
  }

  static auto text_in(const composition::Node& outer, std::string_view label)
      -> std::optional<composition::Rect> {
    std::optional<composition::Rect> found;
    walk(outer, [&](const composition::Node& node) {
      const auto text = node.content_as<components::Text::Content>();
      if (!found.has_value() && text.has_value() &&
          text->get().text() == label && node.presented().has_value()) {
        found = node.presented()->bounds;
      }
    });
    return found;
  }

  static auto radii(const composition::Node& outer) -> std::vector<float> {
    std::vector<float> found;
    walk(outer, [&found](const composition::Node& node) {
      if (const auto& glass = node.modifiers().glass; glass.has_value()) {
        if (const float* points = glass->shape.radius.points()) {
          found.push_back(*points);
        }
      }
      if (const auto& background = node.modifiers().background;
          background.has_value()) {
        if (const float* points = background->shape.radius.points()) {
          found.push_back(*points);
        }
      }
    });
    return found;
  }

  static auto selects(const composition::Node& tab) -> bool {
    bool found = false;
    walk(tab, [&found](const composition::Node& node) {
      const auto& background = node.modifiers().background;
      if (!background.has_value()) {
        return;
      }
      const std::optional<theme::ColourValue> fill = background->fill.colour();
      const theme::ColourRole* role = fill.has_value() ? fill->role() : nullptr;
      found = found ||
              (role != nullptr && *role == structure::DockBar::selection_fill);
    });
    return found;
  }

  static auto has(const std::vector<float>& values, float wanted) -> bool {
    return std::ranges::any_of(values, [wanted](float each) {
      return std::abs(each - wanted) < 0.01F;
    });
  }
};

TEST_F(GalleryPanelsTest, GlassPanelsUseTheNamedRadii) {
  EXPECT_EQ(components::GlassPanel::points(components::PanelRadius::small),
            12.0F);
  EXPECT_EQ(components::GlassPanel::points(components::PanelRadius::medium),
            18.0F);
  EXPECT_EQ(components::GlassPanel::points(components::PanelRadius::large),
            24.0F);
  EXPECT_EQ(components::GlassPanel::points(components::PanelRadius::modal),
            28.0F);
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const components::PanelRadius radius : components::all_panel_radii) {
    const composition::Node* panel =
        keyed(application, Page::panel_key(radius));
    ASSERT_NE(panel, nullptr);
    EXPECT_TRUE(has(radii(*panel), components::GlassPanel::points(radius)))
        << components::GlassPanel::points(radius);
  }
}

TEST_F(GalleryPanelsTest, CardFormKeepsTheGroupedFormMetricsWithPanelCorners) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const composition::Node* form = keyed(application, Page::form_key);
  ASSERT_NE(form, nullptr);
  const std::vector<float> corners = radii(*form);
  EXPECT_TRUE(has(corners, components::GlassPanel::points(
                               structure::FormStyle::card_radius)));
  EXPECT_FALSE(has(corners, theme::metrics::list.grouped_corner_radius));
  const auto first = text_in(*form, "Resolution");
  const auto second = text_in(*form, "Frame rate");
  ASSERT_TRUE(first.has_value() && second.has_value());
  EXPECT_NEAR(second->y - first->y, reference("form.grouped.row_pitch"), 1.0);
  EXPECT_NEAR(theme::metrics::list.grouped_margin,
              reference("form.grouped.group_inset"), 0.5);
}

TEST_F(GalleryPanelsTest, DockTabsTakeTheDockSizesAndSelectByPress) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const composition::Node* dock = keyed(application, Page::dock_key);
  const composition::Node* compact = keyed(application, Page::compact_dock_key);
  ASSERT_NE(dock, nullptr);
  ASSERT_NE(compact, nullptr);
  const composition::Node* second =
      inside(*dock, structure::DockBar::key_of(structure::DockBar::tab_key, 1));
  const composition::Node* small = inside(
      *compact, structure::DockBar::key_of(structure::DockBar::tab_key, 1));
  const composition::Node* settings = inside(
      *dock, structure::DockBar::key_of(structure::DockBar::action_key, 0));
  ASSERT_NE(second, nullptr);
  ASSERT_NE(small, nullptr);
  ASSERT_NE(settings, nullptr);
  EXPECT_NEAR(second->frame().width, structure::DockBar::tab_width, 0.5);
  EXPECT_NEAR(second->frame().height, structure::DockBar::tab_height, 0.5);
  EXPECT_NEAR(small->frame().width, structure::DockBar::icon_tab_width, 0.5);
  EXPECT_NEAR(small->frame().height, structure::DockBar::icon_tab_height, 0.5);
  EXPECT_EQ(dock->frame().height, structure::DockBar::tab_height +
                                      (2.0F * structure::DockBar::padding));
  EXPECT_FALSE(text_in(*compact, "Sound").has_value());
  EXPECT_TRUE(text_in(*dock, "Sound").has_value());
  ASSERT_TRUE(application.root().activate(second->id()));
  run(providers, application, 4);
  const composition::Node* chosen = keyed(application, Page::dock_key);
  ASSERT_NE(chosen, nullptr);
  const composition::Node* selected = inside(
      *chosen, structure::DockBar::key_of(structure::DockBar::tab_key, 1));
  ASSERT_NE(selected, nullptr);
  EXPECT_TRUE(selects(*selected));
  const composition::Node* first = inside(
      *chosen, structure::DockBar::key_of(structure::DockBar::tab_key, 0));
  ASSERT_NE(first, nullptr);
  EXPECT_FALSE(selects(*first));
}

TEST_F(GalleryPanelsTest, TheRailStacksItsTabsAndTheSmallDockIsCompact) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const composition::Node* rail = keyed(application, Page::rail_key);
  const composition::Node* small = keyed(application, Page::small_dock_key);
  ASSERT_NE(rail, nullptr);
  ASSERT_NE(small, nullptr);
  EXPECT_NEAR(rail->frame().height, Page::rail_height, 0.5);
  const composition::Node* first =
      inside(*rail, structure::DockBar::key_of(structure::DockBar::tab_key, 0));
  const composition::Node* second =
      inside(*rail, structure::DockBar::key_of(structure::DockBar::tab_key, 1));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_GT(second->frame().y, first->frame().y);
  const composition::Node* compact = inside(
      *small, structure::DockBar::key_of(structure::DockBar::tab_key, 0));
  ASSERT_NE(compact, nullptr);
  EXPECT_NEAR(compact->frame().width, structure::DockBar::compact_tab_width,
              0.5);
  EXPECT_NE(keyed(application, Page::disclosure_key), nullptr);
  const composition::Node* board = keyed(application, Page::callout_key);
  ASSERT_NE(board, nullptr);
  EXPECT_NE(inside(*board, structure::CalloutBoard::lines_key), nullptr);
}

TEST_F(GalleryPanelsTest, SegmentedProgressMarksTheCurrentStep) {
  EXPECT_EQ(controls::ProgressViewStyle::current_segment(4, 0.25), 1U);
  EXPECT_EQ(controls::ProgressViewStyle::current_segment(4, 0.0), 0U);
  EXPECT_EQ(controls::ProgressViewStyle::current_segment(4, 1.0), 4U);
  EXPECT_EQ(controls::ProgressViewStyle::current_segment(4, std::nullopt),
            std::nullopt);
  EXPECT_EQ(controls::ProgressViewStyle::current_segment(0, 0.5), std::nullopt);
  EXPECT_EQ(controls::ProgressViewStyle::segmented(4),
            controls::ProgressViewStyle::segmented(4));
  EXPECT_FALSE(controls::ProgressViewStyle::segmented(4) ==
               controls::ProgressViewStyle::segmented(5));
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const auto widths = [&application] {
    std::vector<float> found;
    const composition::Node* bar = keyed(application, Page::progress_key);
    if (bar == nullptr) {
      return found;
    }
    walk(*bar, [&found](const composition::Node& node) {
      if (node.modifiers().background.has_value() &&
          std::abs(node.frame().height -
                   controls::ProgressViewStyle::segment_thickness) < 0.01F) {
        found.push_back(node.frame().width);
      }
    });
    return found;
  };
  const std::vector<float> before = widths();
  ASSERT_EQ(before.size(), Page::steps);
  EXPECT_NEAR(before[0], controls::ProgressViewStyle::segment_width, 0.5);
  EXPECT_NEAR(before[1], controls::ProgressViewStyle::current_segment_width,
              0.5);
  EXPECT_NEAR(before[2], controls::ProgressViewStyle::segment_width, 0.5);
  const composition::Node* advance = keyed(application, Page::advance_key);
  ASSERT_NE(advance, nullptr);
  ASSERT_TRUE(application.root().activate(advance->id()));
  for (int index = 0; index < 40; ++index) {
    run(providers, application, 1);
  }
  const std::vector<float> after = widths();
  ASSERT_EQ(after.size(), Page::steps);
  EXPECT_NEAR(after[1], controls::ProgressViewStyle::segment_width, 0.5);
  EXPECT_NEAR(after[2], controls::ProgressViewStyle::current_segment_width,
              0.5);
}

TEST_F(GalleryPanelsTest, SegmentedProgressMarksEveryWholeStep) {
  constexpr std::array<std::size_t, 8> counts{1, 2, 3, 7, 10, 49, 97, 100};
  for (const std::size_t count : counts) {
    for (std::size_t step = 0; step <= count; ++step) {
      const std::optional<double> fraction = controls::ProgressView::fraction(
          static_cast<double>(step), static_cast<double>(count));
      ASSERT_TRUE(fraction.has_value());
      EXPECT_EQ(controls::ProgressViewStyle::current_segment(count, fraction),
                step)
          << step << " of " << count;
    }
  }
}

TEST_F(GalleryPanelsTest, NotANumberProgressHasNoCurrentSegment) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  EXPECT_EQ(controls::ProgressViewStyle::current_segment(4, nan), std::nullopt);
  EXPECT_EQ(controls::ProgressView::fraction(nan, 10.0), std::nullopt);
  EXPECT_EQ(controls::ProgressView::fraction(5.0, nan), std::nullopt);
  EXPECT_EQ(controls::ProgressView::fraction(inf, inf), std::nullopt);
  EXPECT_EQ(controls::ProgressView::fraction(5.0, 10.0), 0.5);
}

TEST_F(GalleryPanelsTest, GlassPanelKeepsIdentityVisibleToItsParent) {
  const composition::Element keyed_panel =
      components::Text("Keyed").id("keyed").glass_panel(
          components::PanelRadius::small);
  ASSERT_TRUE(keyed_panel.key().has_value());
  EXPECT_EQ(*keyed_panel.key(), composition::ElementKey{"keyed"});
  const controls::Picker::Selection selection{
      .is_selected =
          [](const composition::TagValue& tag) { return *tag.get<int>() == 2; },
      .select = [](const composition::TagValue&) {}};
  const std::vector<controls::PickerStyle::Option> options =
      controls::Picker::options(
          [] {
            return components::Group(components::Text("One").tag(1).glass_panel(
                                         components::PanelRadius::small),
                                     components::Text("Two").tag(2).glass_panel(
                                         components::PanelRadius::medium));
          },
          selection);
  ASSERT_EQ(options.size(), 2U);
  EXPECT_FALSE(options[0].is_selected);
  EXPECT_TRUE(options[1].is_selected);
}

TEST_F(GalleryPanelsTest, HandheldMatchesGolden) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      rendered(providers, theme::ColorScheme::light, false);
  matches_golden(providers, "panels_handheld_1280x720");
}

TEST_F(GalleryPanelsTest, HandheldDarkMatchesGolden) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      rendered(providers, theme::ColorScheme::dark, false);
  matches_golden(providers, "panels_handheld_dark_1280x720");
}

TEST_F(GalleryPanelsTest, DockedMatchesGolden) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      rendered(providers, theme::ColorScheme::light, true);
  matches_golden(providers, "panels_docked_1920x1080");
}

}
