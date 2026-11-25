#include <arisa/appearance/appearance_file.hpp>
#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/theme_file.hpp>

#include <algorithm>
#include <format>
#include <utility>

namespace arisa::appearance {

AppearanceModel::AppearanceModel(AppearancePolicy policy, ThemeLibrary library,
                                 ThemeLibrary::StorageRef storage,
                                 provider::DiagnosticsProvider& diagnostics)
    : shared_(std::make_shared<Shared>(
          Shared{.policy = std::move(policy),
                 .library = std::move(library),
                 .storage = storage,
                 .diagnostics = diagnostics,
                 .overrides = {},
                 .load_error = std::nullopt,
                 .loaded = theme::Theme::defaults(),
                 .edits = {},
                 .animated = true,
                 .stale = false,
                 .theme = theme::Theme::defaults(),
                 .settings = composition::Observable<AppearanceSettings>{}})) {
  restore();
  recompute();
}

auto AppearanceModel::settings() const noexcept
    -> const composition::Observable<AppearanceSettings>& {
  return shared_->settings;
}

auto AppearanceModel::theme() const -> const theme::Theme& {
  if (shared_->stale) {
    shared_->theme = shared_->edits.apply(shared_->loaded);
    shared_->stale = false;
  }
  return shared_->theme;
}

auto AppearanceModel::overrides() const noexcept -> const AppearanceOverrides& {
  return shared_->overrides;
}

auto AppearanceModel::policy() const noexcept -> const AppearancePolicy& {
  return shared_->policy;
}

auto AppearanceModel::load_error() const noexcept
    -> const std::optional<ThemeError>& {
  return shared_->load_error;
}

auto AppearanceModel::theme_names() const -> std::vector<std::string> {
  std::vector<std::string> names = shared_->library.names();
  if (is_locked(AppearanceField::theme)) {
    std::erase_if(names, [this](const std::string& name) {
      return name != shared_->policy.defaults.theme;
    });
  }
  return names;
}

auto AppearanceModel::is_locked(AppearanceField field) const noexcept -> bool {
  return shared_->policy.locks.contains(field);
}

void AppearanceModel::update(const Change& change) const {
  AppearanceOverrides edited = shared_->overrides;
  change(edited);
  if (edited == shared_->overrides) {
    return;
  }
  shared_->overrides = std::move(edited);
  recompute();
  persist();
}

void AppearanceModel::reset() const {
  shared_->overrides = AppearanceOverrides{};
  recompute();
  persist();
}

void AppearanceModel::restore() const {
  if (!shared_->storage.has_value()) {
    return;
  }
  const auto path = provider::AssetPath::create(AppearanceFile::path);
  if (!path.has_value()) {
    return;
  }
  auto blob = shared_->storage->get().read(*path);
  if (!blob.has_value()) {
    return;
  }
  std::string text(blob->size(), '\0');
  std::ranges::transform(blob->bytes(), text.begin(), [](std::byte value) {
    return static_cast<char>(value);
  });
  auto parsed = AppearanceFile::parse(text, AppearanceFile::path);
  if (!parsed.has_value()) {
    shared_->load_error = parsed.error();
    shared_->diagnostics.get().report(provider::Diagnostic{
        .severity = provider::Severity::warning,
        .category = "appearance",
        .message = std::format("{}; using the application defaults",
                               parsed.error().description())});
    return;
  }
  shared_->overrides = std::move(*parsed);
}

auto AppearanceModel::chosen() const -> std::string {
  const AppearancePolicy& policy = shared_->policy;
  return policy.enforce(shared_->overrides.over(policy.defaults)).theme;
}

void AppearanceModel::recompute() const {
  adopt(shared_->library.resolve(chosen(), shared_->diagnostics.get()), true);
}

void AppearanceModel::adopt(theme::Theme resolved, bool animated) const {
  const AppearancePolicy& policy = shared_->policy;
  AppearanceSettings base = policy.defaults;
  const theme::Theme::Preferences& preferred = resolved.preferences();
  if (preferred.dynamic_type_size.has_value()) {
    base.dynamic_type_size = *preferred.dynamic_type_size;
  }
  if (!base.ui_scale.handheld.has_value()) {
    base.ui_scale.handheld = preferred.handheld_scale;
  }
  if (!base.ui_scale.docked.has_value()) {
    base.ui_scale.docked = preferred.docked_scale;
  }
  if (!base.ui_scale.desktop.has_value()) {
    base.ui_scale.desktop = preferred.desktop_scale;
  }
  AppearanceSettings effective =
      policy.enforce(shared_->overrides.over(std::move(base)));
  if (preferred.color_scheme.has_value()) {
    effective.color_scheme = *preferred.color_scheme == theme::ColorScheme::dark
                                 ? ColorSchemePreference::dark
                                 : ColorSchemePreference::light;
  }
  if (effective.accent.has_value()) {
    resolved = resolved.with_accent(*effective.accent);
  }
  shared_->loaded = std::move(resolved);
  publish(std::move(effective), animated);
}

void AppearanceModel::publish(AppearanceSettings effective,
                              bool animated) const {
  shared_->animated = animated;
  shared_->stale = false;
  theme::Theme edited = shared_->edits.apply(shared_->loaded);
  const bool moved = edited != shared_->theme;
  shared_->theme = std::move(edited);
  if (moved && effective == shared_->settings.get()) {
    shared_->settings.update([](AppearanceSettings&) {});
    return;
  }
  shared_->settings.set(std::move(effective));
}

auto AppearanceModel::animated() const noexcept -> bool {
  return shared_->animated;
}

auto AppearanceModel::base_theme() const noexcept -> const theme::Theme& {
  return shared_->loaded;
}

auto AppearanceModel::edits() const noexcept -> const ThemeEdits& {
  return shared_->edits;
}

void AppearanceModel::revise() const {
  shared_->animated = false;
  shared_->stale = true;
  shared_->settings.update([](AppearanceSettings&) {});
}

auto AppearanceModel::current(const ThemeToken& token) const -> TokenValue {
  if (shared_->stale) {
    if (std::optional<TokenValue> pending = shared_->edits.value(token)) {
      return *std::move(pending);
    }
  }
  return token.read(theme());
}

auto AppearanceModel::edit(const ThemeToken& token,
                           const TokenValue& value) const -> bool {
  if (token.clamp(value) == std::optional{current(token)} ||
      !shared_->edits.set(token, value)) {
    return false;
  }
  revise();
  return true;
}

auto AppearanceModel::undo(const ThemeToken& token) const -> bool {
  if (!shared_->edits.undo(token)) {
    return false;
  }
  revise();
  return true;
}

void AppearanceModel::reset_edits() const {
  if (shared_->edits.empty()) {
    return;
  }
  shared_->edits.reset();
  revise();
}

auto AppearanceModel::reload() const -> std::expected<void, ThemeError> {
  auto loaded = shared_->library.load(chosen(), ThemeLibrary::Fallback::none);
  if (!loaded.has_value()) {
    shared_->load_error = loaded.error();
    shared_->diagnostics.get().report(provider::Diagnostic{
        .severity = provider::Severity::warning,
        .category = "theme",
        .message = std::format("{}; keeping the previous theme",
                               loaded.error().description())});
    return std::unexpected(loaded.error());
  }
  shared_->load_error.reset();
  adopt(std::move(*loaded), false);
  return {};
}

auto AppearanceModel::watched() const -> std::vector<ThemeLibrary::Location> {
  return shared_->library.locations(chosen());
}

auto AppearanceModel::library() const noexcept -> const ThemeLibrary& {
  return shared_->library;
}

auto AppearanceModel::export_edits() const
    -> std::expected<Export, ThemeError> {
  const ThemeLibrary::StorageRef destination = shared_->library.destination();
  if (!destination.has_value()) {
    return std::unexpected(ThemeError{ThemeErrorCode::io_failure,
                                      std::string{ThemeLibrary::user_directory},
                                      0U,
                                      {},
                                      "no storage to export the theme to"});
  }
  return export_edits(destination->get());
}

auto AppearanceModel::export_edits(provider::StorageProvider& destination) const
    -> std::expected<Export, ThemeError> {
  const std::string current = chosen();
  const bool flattened = current.ends_with(edited_suffix);
  const std::string name =
      flattened ? current : std::format("{}{}", current, edited_suffix);
  std::string extends = current;
  theme::Theme base = shared_->loaded;
  if (flattened) {
    extends = shared_->library.base_of(current).value_or(
        std::string{ThemeLibrary::default_name});
    auto parent = shared_->library.load(extends);
    if (!parent.has_value()) {
      return std::unexpected(parent.error());
    }
    base = std::move(*parent);
    if (const auto& accent = shared_->settings.get().accent) {
      base = base.with_accent(*accent);
    }
  }
  std::string text = ThemeFile::serialise(theme(), base, name, extends);
  const std::string file = std::format("{}/{}{}", ThemeLibrary::user_directory,
                                       name, ThemeLibrary::extension);
  const auto path = provider::AssetPath::create(file);
  const auto folder = provider::AssetPath::create(ThemeLibrary::user_directory);
  if (!path.has_value() || !folder.has_value()) {
    return std::unexpected(ThemeError{ThemeErrorCode::io_failure,
                                      file,
                                      0U,
                                      {},
                                      "not a valid theme file name"});
  }
  if (auto made = destination.create_directories(*folder); !made.has_value()) {
    return std::unexpected(ThemeError{
        ThemeErrorCode::io_failure,
        destination.location(*folder),
        0U,
        {},
        reason("the theme folder could not be created", made.error())});
  }
  if (auto written = destination.write(*path, std::as_bytes(std::span{text}));
      !written.has_value()) {
    return std::unexpected(
        ThemeError{ThemeErrorCode::io_failure,
                   destination.location(*path),
                   0U,
                   {},
                   reason("the theme could not be written", written.error())});
  }
  return Export{.name = name,
                .file = file,
                .location = destination.location(*path),
                .text = std::move(text)};
}

auto AppearanceModel::reason(std::string_view action,
                             const provider::ProviderError& error)
    -> std::string {
  if (const auto native = error.native_result()) {
    return std::format("{}: {} (error {})", action, error.description(),
                       *native);
  }
  return std::format("{}: {}", action, error.description());
}

void AppearanceModel::persist() const {
  if (!shared_->storage.has_value()) {
    return;
  }
  const auto path = provider::AssetPath::create(AppearanceFile::path);
  if (!path.has_value()) {
    return;
  }
  const std::string text = AppearanceFile::serialise(shared_->overrides);
  if (auto written =
          shared_->storage->get().write(*path, std::as_bytes(std::span{text}));
      !written.has_value()) {
    shared_->diagnostics.get().report(provider::Diagnostic{
        .severity = provider::Severity::warning,
        .category = "appearance",
        .message = "appearance settings could not be saved"});
  }
}

}
