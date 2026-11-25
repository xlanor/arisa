#pragma once

#include <arisa/components/lazy_stack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/interactions.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/presented_frame.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/seed.hpp>

#include <gallery_fonts.hpp>
#include <gallery_page.hpp>
#include <gallery_screen.hpp>
#include <scroll_fixture.hpp>

#include <gtest/gtest.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class GalleryFixture : public ScrollFixture {
 protected:
  using Visit = std::function<void(const composition::Node&)>;

  using AssetBytes =
      std::vector<std::pair<std::string_view, std::vector<std::byte>>>;

  static auto asset_bytes() -> const AssetBytes& {
    static const AssetBytes loaded = [] {
      AssetBytes read;
      for (const std::string_view file :
           {std::string_view{"arisa/fonts/InterVariable.ttf"},
            std::string_view{"arisa/fonts/MaterialSymbolsRounded.ttf"},
            std::string_view{"arisa/fonts/MaterialSymbolsRoundedFilled.ttf"},
            std::string_view{"arisa/images/badge_48.png"},
            std::string_view{"arisa/images/pixel_8.png"},
            std::string_view{"arisa/themes/arisa-theme-glass.toml"}}) {
        std::ifstream stream{std::filesystem::path{ARISA_SOURCE_ASSETS} / file,
                             std::ios::binary};
        const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                    std::istreambuf_iterator<char>{}};
        std::vector<std::byte> bytes(raw.size());
        std::ranges::transform(raw, bytes.begin(),
                               [](char value) { return std::byte(value); });
        read.emplace_back(file, std::move(bytes));
      }
      return read;
    }();
    return loaded;
  }

  static void load_fonts(
      provider::headless::HeadlessProviderFactory& providers) {
    for (const auto& [file, bytes] : asset_bytes()) {
      ASSERT_FALSE(bytes.empty()) << file;
      auto path = provider::AssetPath::create(file);
      ASSERT_TRUE(path.has_value());
      providers.assets().add(std::move(*path), bytes);
    }
  }

  static constexpr int level_threshold = 16;
  static constexpr double maximum_off_fraction = 0.0005;
  static constexpr double maximum_mean_difference = 0.1;

  static auto updating() -> bool {
    const char* value = std::getenv("ARISA_UPDATE_GOLDENS");
    return value != nullptr && std::string_view{value} == "1";
  }

  static auto displaced(const unsigned char* stored, std::uint32_t width,
                        std::uint32_t height, std::span<const std::byte> seen,
                        std::uint32_t x, std::uint32_t y) -> bool {
    const std::size_t at = ((std::size_t{y} * width) + x) * 4;
    for (std::uint32_t row = y == 0 ? 0 : y - 1;
         row <= std::min(y + 1, height - 1); ++row) {
      for (std::uint32_t column = x == 0 ? 0 : x - 1;
           column <= std::min(x + 1, width - 1); ++column) {
        const std::size_t other = ((std::size_t{row} * width) + column) * 4;
        bool close = true;
        for (std::size_t channel = 0; channel < 3; ++channel) {
          close = close &&
                  std::abs(std::to_integer<int>(seen[at + channel]) -
                           int{stored[other + channel]}) <= level_threshold;
        }
        if (close) {
          return true;
        }
      }
    }
    return false;
  }

  static void matches_golden(
      provider::headless::HeadlessProviderFactory& providers,
      std::string_view name) {
    const auto image = providers.gpu().capture();
    ASSERT_TRUE(image.has_value());
    const std::filesystem::path path =
        std::filesystem::path{ARISA_GALLERY_GOLDENS} /
        (std::string{name} + ".png");
    const auto width = static_cast<int>(image->width());
    const auto height = static_cast<int>(image->height());
    if (updating()) {
      ASSERT_NE(stbi_write_png(path.string().c_str(), width, height, 4,
                               image->pixels().data(), width * 4),
                0);
      return;
    }
    std::ifstream file{path, std::ios::binary};
    ASSERT_TRUE(file) << path << " is missing; run make update-goldens";
    const std::vector<char> bytes{std::istreambuf_iterator<char>{file},
                                  std::istreambuf_iterator<char>{}};
    int stored_width = 0;
    int stored_height = 0;
    int channels = 0;
    const std::unique_ptr<stbi_uc, void (*)(void*)> stored{
        stbi_load_from_memory(
            static_cast<const stbi_uc*>(static_cast<const void*>(bytes.data())),
            static_cast<int>(bytes.size()), &stored_width, &stored_height,
            &channels, 4),
        &stbi_image_free};
    ASSERT_NE(stored, nullptr) << path;
    ASSERT_EQ(stored_width, width) << name;
    ASSERT_EQ(stored_height, height) << name;
    const std::span<const std::byte> seen = image->pixels();
    std::size_t off = 0;
    double total = 0.0;
    for (std::uint32_t y = 0; y < image->height(); ++y) {
      for (std::uint32_t x = 0; x < image->width(); ++x) {
        const std::size_t at = ((std::size_t{y} * image->width()) + x) * 4;
        int worst = 0;
        for (std::size_t channel = 0; channel < 3; ++channel) {
          const int difference =
              std::abs(std::to_integer<int>(seen[at + channel]) -
                       int{stored.get()[at + channel]});
          total += difference;
          worst = std::max(worst, difference);
        }
        if (worst > level_threshold &&
            !displaced(stored.get(), image->width(), image->height(), seen, x,
                       y)) {
          ++off;
        }
      }
    }
    const auto pixels = static_cast<double>(image->width()) * image->height();
    EXPECT_LE(static_cast<double>(off), pixels * maximum_off_fraction) << name;
    EXPECT_LE(total / (pixels * 3.0), maximum_mean_difference) << name;
  }

  static auto start_gallery(
      provider::headless::HeadlessProviderFactory& providers,
      examples::GalleryPage page,
      theme::ColorScheme scheme = theme::ColorScheme::light,
      provider::SurfaceSize size =
          provider::headless::ScriptedSurface::default_size,
      std::optional<AppearanceOptions> appearance = std::nullopt,
      std::optional<float> scale = std::nullopt) -> Application {
    load_fonts(providers);
    providers.surface().resize(size);
    providers.surface().set_scale(
        scale.has_value() ? provider::SurfaceScale::create(*scale).value_or(
                                provider::SurfaceScale::for_pixels(size))
                          : provider::SurfaceScale::for_pixels(size));
    auto application = Application::create(
        providers, examples::GalleryScreen{page, theme::Seed{.scheme = scheme}},
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = examples::GalleryFonts::load(providers.assets()),
            .atlas = {},
            .interaction = {},
            .appearance = std::move(appearance)});
    EXPECT_TRUE(application.has_value());
    Application started = std::move(*application);
    run(providers, started, 4);
    return started;
  }

  static void run(provider::headless::HeadlessProviderFactory& providers,
                  Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
      ASSERT_TRUE(application.frame().has_value());
    }
  }

  static void gather(const composition::Node& node, bool disabled,
                     std::vector<composition::NodeId>& found) {
    const bool top = !node.parent().has_value();
    if (!top && node.phase() != composition::NodeLifecyclePhase::live) {
      return;
    }
    const composition::Interactions& declared = node.interactions();
    const bool off = disabled || declared.disabled;
    const std::size_t mark = found.size();
    const bool focusable =
        !top && !off &&
        declared.focusable.value_or(node.content().activatable());
    if (focusable) {
      found.push_back(node.id());
    }
    for (const composition::Node& child : node.children()) {
      gather(child, off, found);
    }
    if (focusable && declared.focus_fallback && found.size() > mark + 1) {
      found.erase(found.begin() + static_cast<std::ptrdiff_t>(mark));
    }
  }

  static void walk(const composition::Node& node, const Visit& visit) {
    if (node.phase() == composition::NodeLifecyclePhase::departing) {
      return;
    }
    visit(node);
    for (const composition::Node& child : node.children()) {
      walk(child, visit);
    }
  }

  static auto presented(const composition::Node& node)
      -> std::vector<composition::PresentedFrame> {
    std::vector<composition::PresentedFrame> found;
    walk(node, [&found](const composition::Node& each) {
      if (each.presented().has_value()) {
        found.push_back(*each.presented());
      }
    });
    return found;
  }

  static auto visible(const composition::Node& node, float width, float height)
      -> bool {
    if (!node.presented().has_value() || !node.presented()->visible) {
      return false;
    }
    const composition::Rect& shown = node.presented()->bounds;
    composition::Rect area{
        .x = 0.0F, .y = 0.0F, .width = width, .height = height};
    if (node.presented()->clip.has_value()) {
      area = *node.presented()->clip;
    }
    return shown.y + shown.height > area.y && shown.y < area.y + area.height &&
           shown.x + shown.width > area.x && shown.x < area.x + area.width;
  }

  static auto focusables(Application& application)
      -> std::vector<composition::NodeId> {
    std::vector<composition::NodeId> found;
    gather(application.root().tree().root(), false, found);
    return found;
  }

  static auto opposite(provider::Button button) noexcept -> provider::Button {
    switch (button) {
      case provider::Button::dpad_up:
        return provider::Button::dpad_down;
      case provider::Button::dpad_down:
        return provider::Button::dpad_up;
      case provider::Button::dpad_left:
        return provider::Button::dpad_right;
      default:
        return provider::Button::dpad_left;
    }
  }

  static auto within(Application& application, composition::NodeId id,
                     std::string_view container) -> bool {
    const composition::Tree& tree = application.root().tree();
    const auto boundary = tree.find(composition::ElementKey{container});
    std::optional<composition::NodeId> at = id;
    while (at.has_value() && boundary.has_value()) {
      if (*at == *boundary) {
        return true;
      }
      const auto node = tree.node(*at);
      if (!node.has_value()) {
        return false;
      }
      at = node->get().parent();
    }
    return false;
  }

  static auto in_page(Application& application, composition::NodeId id)
      -> bool {
    return within(application, id, structure::Scaffold::content_key) &&
           !within(application, id, "arisa.tab.sidebar");
  }

  static constexpr std::array directions{
      provider::Button::dpad_down, provider::Button::dpad_right,
      provider::Button::dpad_up, provider::Button::dpad_left};

  using Edges = std::map<composition::NodeId,
                         std::array<std::optional<composition::NodeId>, 4>>;

  struct Traversal {
    std::vector<composition::NodeId> reached;
    Edges edges;
  };

  static auto travel(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, const Edges& edges,
                     composition::NodeId target) -> bool {
    for (int hop = 0; hop < 64; ++hop) {
      const auto here = application.interaction().focused();
      if (!here.has_value()) {
        return false;
      }
      if (*here == target) {
        return true;
      }
      std::map<composition::NodeId, std::size_t> first;
      std::deque<composition::NodeId> queue{*here};
      std::map<composition::NodeId, bool> seen{{*here, true}};
      std::optional<std::size_t> step;
      while (!queue.empty() && !step.has_value()) {
        const composition::NodeId at = queue.front();
        queue.pop_front();
        const auto found = edges.find(at);
        if (found == edges.end()) {
          continue;
        }
        for (std::size_t index = 0; index < directions.size(); ++index) {
          const auto next = found->second[index];
          if (!next.has_value() || seen.contains(*next)) {
            continue;
          }
          seen[*next] = true;
          first[*next] = at == *here ? index : first[at];
          if (*next == target) {
            step = first[*next];
            break;
          }
          queue.push_back(*next);
        }
      }
      if (!step.has_value()) {
        return false;
      }
      press(providers, application, directions[*step]);
      run(providers, application, 2);
    }
    return false;
  }

  static constexpr std::size_t lazy_sample = 12;

  static auto lazy(Application& application, composition::NodeId id) -> bool {
    const composition::Tree& tree = application.root().tree();
    std::optional<composition::NodeId> at = id;
    while (at.has_value()) {
      const auto node = tree.node(*at);
      if (!node.has_value()) {
        return false;
      }
      if (node->get()
              .content_as<components::LazyStack::Content>()
              .has_value()) {
        return true;
      }
      at = node->get().parent();
    }
    return false;
  }

  static auto traverse(provider::headless::HeadlessProviderFactory& providers,
                       Application& application) -> Traversal {
    Edges edges;
    std::vector<composition::NodeId> reached;
    const auto note = [&reached](composition::NodeId id) {
      if (!std::ranges::contains(reached, id)) {
        reached.push_back(id);
      }
    };
    std::vector<composition::NodeId> lazy_rows;
    for (int guard = 0; guard < 2000; ++guard) {
      const auto here = application.interaction().focused();
      if (!here.has_value()) {
        break;
      }
      note(*here);
      auto& known = edges[*here];
      if (lazy(application, *here)) {
        if (!std::ranges::contains(lazy_rows, *here)) {
          lazy_rows.push_back(*here);
        }
        if (lazy_rows.size() >= lazy_sample && !known[0].has_value()) {
          known[0] = *here;
        }
      }
      const auto untried = std::ranges::find_if(
          known, [](const auto& edge) { return !edge.has_value(); });
      std::optional<std::size_t> step;
      if (untried != known.end()) {
        step = static_cast<std::size_t>(untried - known.begin());
      } else {
        std::map<composition::NodeId, std::size_t> first;
        std::deque<composition::NodeId> queue{*here};
        std::map<composition::NodeId, bool> seen{{*here, true}};
        while (!queue.empty() && !step.has_value()) {
          const composition::NodeId at = queue.front();
          queue.pop_front();
          for (std::size_t index = 0; index < directions.size(); ++index) {
            const auto next = edges[at][index];
            if (!next.has_value() || seen.contains(*next)) {
              continue;
            }
            seen[*next] = true;
            first[*next] = at == *here ? index : first[at];
            const auto& further = edges[*next];
            if (std::ranges::any_of(
                    further,
                    [](const auto& edge) { return !edge.has_value(); }) ||
                !edges.contains(*next)) {
              step = first[*next];
              break;
            }
            queue.push_back(*next);
          }
        }
        if (!step.has_value()) {
          break;
        }
      }
      press(providers, application, directions[*step]);
      run(providers, application, 2);
      const auto there = application.interaction().focused();
      if (!there.has_value()) {
        break;
      }
      if (!in_page(application, *there)) {
        edges[*here][*step] = *here;
        for (const provider::Button back :
             {opposite(directions[*step]), provider::Button::dpad_right,
              provider::Button::dpad_down}) {
          const auto now = application.interaction().focused();
          if (now.has_value() && in_page(application, *now)) {
            break;
          }
          press(providers, application, back);
          run(providers, application, 2);
        }
        continue;
      }
      edges[*here][*step] = *there;
      note(*there);
    }
    return Traversal{.reached = std::move(reached), .edges = std::move(edges)};
  }

  static void scroll_by(provider::headless::HeadlessProviderFactory& providers,
                        Application& application, provider::PixelPoint from,
                        float distance) {
    touch_down(providers, application, from);
    constexpr int steps = 12;
    for (int index = 1; index <= steps; ++index) {
      touch_move(providers, application,
                 provider::PixelPoint{
                     .x = from.x,
                     .y = from.y - (distance * static_cast<float>(index) /
                                    static_cast<float>(steps))},
                 std::chrono::milliseconds{40});
    }
    touch_up(providers, application,
             provider::PixelPoint{.x = from.x, .y = from.y - distance},
             std::chrono::milliseconds{200});
    run(providers, application, 90);
  }
};

}
