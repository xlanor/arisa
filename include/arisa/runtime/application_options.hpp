#pragma once

#include <arisa/appearance/appearance_policy.hpp>
#include <arisa/async/executors.hpp>
#include <arisa/components/image_cache.hpp>
#include <arisa/interaction/interaction_options.hpp>
#include <arisa/provider/storage/provider.hpp>
#include <arisa/text/atlas_options.hpp>
#include <arisa/text/font_entry.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/theme/colour.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace arisa::runtime {

struct AppearanceOptions {
  appearance::AppearancePolicy policy{};
  std::optional<std::reference_wrapper<provider::StorageProvider>> storage{};
  std::optional<std::reference_wrapper<provider::StorageProvider>> themes{};
  std::optional<std::chrono::milliseconds> hot_reload{};
};

struct ImageOptions {
  components::ImageCache::Budget decoded{
      components::ImageCache::default_budget};
  std::uint64_t textures{components::ImageCache::default_budget.value()};
};

struct StorageOptions {
  std::optional<std::reference_wrapper<provider::StorageProvider>> provider{};
  std::string app_file{"app_storage.toml"};
  std::string scene_file{"scene_storage.toml"};
};

struct LocalizationOptions {
  std::optional<text::Locale> locale{};
  std::string strings{"strings"};
};

struct LinkOptions {
  std::vector<std::string> claimed{};
  std::optional<std::string> launch_url{};
};

struct ApplicationOptions {
  std::optional<async::Executors> executors;
  std::vector<text::FontEntry> fonts;
  text::AtlasOptions atlas{};
  interaction::InteractionOptions interaction{};
  std::optional<AppearanceOptions> appearance{};
  std::optional<theme::Colour> launch_colour{};
  bool debug_glass_coverage{false};
  bool debug_full_walk{false};
  bool debug_separate_backdrops{false};
  bool render_thread{true};
  bool pacing{true};
  ImageOptions images{};
  StorageOptions storage{};
  LocalizationOptions localization{};
  std::optional<std::reference_wrapper<provider::StorageProvider>> files{};
  LinkOptions links{};
};

}
