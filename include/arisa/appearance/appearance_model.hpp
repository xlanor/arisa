#pragma once

#include <arisa/appearance/appearance_policy.hpp>
#include <arisa/appearance/appearance_settings.hpp>
#include <arisa/appearance/theme_edits.hpp>
#include <arisa/appearance/theme_error.hpp>
#include <arisa/appearance/theme_library.hpp>
#include <arisa/appearance/theme_token.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/storage/provider.hpp>
#include <arisa/theme/theme.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::appearance {

class AppearanceModel {
 public:
  using Change = std::function<void(AppearanceOverrides&)>;

  struct Export {
    std::string name;
    std::string file;
    std::string location;
    std::string text;
  };

  static constexpr std::string_view edited_suffix = "-edited";

  AppearanceModel(AppearancePolicy policy, ThemeLibrary library,
                  ThemeLibrary::StorageRef storage,
                  provider::DiagnosticsProvider& diagnostics);

  auto settings() const noexcept
      -> const composition::Observable<AppearanceSettings>&;

  auto theme() const -> const theme::Theme&;

  auto overrides() const noexcept -> const AppearanceOverrides&;

  auto policy() const noexcept -> const AppearancePolicy&;

  auto load_error() const noexcept -> const std::optional<ThemeError>&;

  auto theme_names() const -> std::vector<std::string>;

  auto is_locked(AppearanceField field) const noexcept -> bool;

  void update(const Change& change) const;

  void reset() const;

  auto animated() const noexcept -> bool;

  auto base_theme() const noexcept -> const theme::Theme&;

  auto edits() const noexcept -> const ThemeEdits&;

  auto edit(const ThemeToken& token, const TokenValue& value) const -> bool;

  auto undo(const ThemeToken& token) const -> bool;

  void reset_edits() const;

  [[nodiscard]] auto reload() const -> std::expected<void, ThemeError>;

  auto watched() const -> std::vector<ThemeLibrary::Location>;

  auto library() const noexcept -> const ThemeLibrary&;

  [[nodiscard]] auto export_edits() const -> std::expected<Export, ThemeError>;

  [[nodiscard]] auto export_edits(provider::StorageProvider& destination) const
      -> std::expected<Export, ThemeError>;

  friend auto operator==(const AppearanceModel& left,
                         const AppearanceModel& right) -> bool {
    return left.shared_ == right.shared_;
  }

 private:
  struct Shared {
    AppearancePolicy policy;
    ThemeLibrary library;
    ThemeLibrary::StorageRef storage;
    std::reference_wrapper<provider::DiagnosticsProvider> diagnostics;
    AppearanceOverrides overrides;
    std::optional<ThemeError> load_error;
    theme::Theme loaded;
    ThemeEdits edits;
    bool animated{true};
    bool stale{false};
    theme::Theme theme;
    composition::Observable<AppearanceSettings> settings;
  };

  void restore() const;

  void recompute() const;

  auto chosen() const -> std::string;

  void adopt(theme::Theme resolved, bool animated) const;

  void publish(AppearanceSettings effective, bool animated) const;

  void revise() const;

  auto current(const ThemeToken& token) const -> TokenValue;

  void persist() const;

  static auto reason(std::string_view action,
                     const provider::ProviderError& error) -> std::string;

  std::shared_ptr<Shared> shared_;
};

}
