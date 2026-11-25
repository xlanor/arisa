#include <arisa/composition/user_defaults.hpp>

#include <toml.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <sstream>
#include <utility>

namespace arisa::composition {

struct UserDefaults::State {
  Observable<Values> values;
  std::optional<std::reference_wrapper<provider::StorageProvider>> storage;
  std::optional<provider::AssetPath> path;
  bool pending{false};
};

const EnvironmentKey<std::optional<UserDefaults>> UserDefaults::app_storage{
    "app-storage", std::nullopt};

const EnvironmentKey<std::optional<UserDefaults>> UserDefaults::scene_storage{
    "scene-storage", std::nullopt};

UserDefaults::UserDefaults() : state_(std::make_shared<State>()) {}

UserDefaults::UserDefaults(std::shared_ptr<State> state) noexcept
    : state_(std::move(state)) {}

auto UserDefaults::open(provider::StorageProvider& storage,
                        provider::AssetPath path)
    -> std::expected<UserDefaults, provider::ProviderError> {
  auto state = std::make_shared<State>();
  auto blob = storage.read(path);
  if (blob.has_value()) {
    std::string text(blob->size(), '\0');
    std::ranges::transform(blob->bytes(), text.begin(), [](std::byte value) {
      return static_cast<char>(value);
    });
    std::optional<Values> parsed = parse(text, path.value());
    if (!parsed.has_value()) {
      return std::unexpected(provider::ProviderError{
          provider::ProviderErrorCode::invalid_argument});
    }
    state->values.set(std::move(*parsed));
  } else if (blob.error().code() != provider::ProviderErrorCode::not_found) {
    return std::unexpected(blob.error());
  }
  state->storage = storage;
  state->path = std::move(path);
  return UserDefaults{std::move(state)};
}

auto UserDefaults::parse(std::string_view text, std::string_view source)
    -> std::optional<Values> {
  toml::parse_result parsed = toml::parse(text, source);
  if (!parsed) {
    return std::nullopt;
  }
  Values found;
  for (const auto& [key, node] : parsed.table()) {
    std::string name{key.str()};
    if (const auto* flag = node.as_boolean()) {
      found.emplace(std::move(name), Value{flag->get()});
    } else if (const auto* number = node.as_integer()) {
      found.emplace(std::move(name), Value{number->get()});
    } else if (const auto* real = node.as_floating_point()) {
      found.emplace(std::move(name), Value{real->get()});
    } else if (const auto* words = node.as_string()) {
      found.emplace(std::move(name), Value{words->get()});
    }
  }
  return found;
}

auto UserDefaults::serialise(const Values& values) -> std::string {
  toml::table document;
  for (const auto& [key, stored] : values) {
    std::visit(
        [&document, &key](const auto& held) { document.insert(key, held); },
        stored);
  }
  std::ostringstream stream;
  stream << document << '\n';
  return std::move(stream).str();
}

auto UserDefaults::value(std::string_view key) const -> std::optional<Value> {
  const Values& held = state_->values.get();
  const auto found = held.find(key);
  if (found == held.end()) {
    return std::nullopt;
  }
  return found->second;
}

void UserDefaults::set(std::string_view key, Value value) const {
  const Values& held = state_->values.get();
  if (const auto found = held.find(key);
      found != held.end() && found->second == value) {
    return;
  }
  state_->values.update(channel(key), [key, &value](Values& values) {
    values.insert_or_assign(std::string{key}, std::move(value));
  });
  state_->pending = true;
}

void UserDefaults::remove(std::string_view key) const {
  const Values& held = state_->values.get();
  if (!held.contains(key)) {
    return;
  }
  state_->values.update(channel(key), [key](Values& values) {
    if (const auto found = values.find(key); found != values.end()) {
      values.erase(found);
    }
  });
  state_->pending = true;
}

auto UserDefaults::values() const noexcept -> const Observable<Values>& {
  return state_->values;
}

auto UserDefaults::channel(std::string_view key) noexcept -> std::uintptr_t {
  return std::hash<std::string_view>{}(key);
}

auto UserDefaults::pending() const noexcept -> bool { return state_->pending; }

auto UserDefaults::flush() const
    -> std::expected<void, provider::ProviderError> {
  if (!state_->pending || !state_->storage.has_value() ||
      !state_->path.has_value()) {
    return {};
  }
  const std::string text = serialise(state_->values.get());
  if (auto written = state_->storage->get().write(
          *state_->path, std::as_bytes(std::span{text}));
      !written.has_value()) {
    return std::unexpected(written.error());
  }
  state_->pending = false;
  return {};
}

}
