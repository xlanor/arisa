#pragma once

#include <arisa/composition/accessibility_focus_state.hpp>
#include <arisa/composition/accessibility_settings.hpp>
#include <arisa/composition/app_storage.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/environment_object.hpp>
#include <arisa/composition/environment_override.hpp>
#include <arisa/composition/environment_values.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/focused_binding.hpp>
#include <arisa/composition/focused_object.hpp>
#include <arisa/composition/focused_value_key.hpp>
#include <arisa/composition/focused_values.hpp>
#include <arisa/composition/gesture_state.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/lazy_state.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/node_anchor.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/root_services.hpp>
#include <arisa/composition/scene_storage.hpp>
#include <arisa/composition/scroll_view_proxy.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/state_slot.hpp>
#include <arisa/composition/ui_error.hpp>
#include <arisa/composition/user_defaults.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/transaction.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::composition {

class Node;
class Tree;

class Context {
 public:
  Context(const Context&) = delete;
  auto operator=(const Context&) -> Context& = delete;
  Context(Context&&) = delete;
  auto operator=(Context&&) -> Context& = delete;
  ~Context() = default;

  template <class T>
  auto state(const StateKey<T>& key) -> StateHandle<T> {
    auto slot = std::static_pointer_cast<StateSlot::Typed<T>>(
        slot_for(key.identity(), [&key] {
          return std::make_shared<StateSlot::Typed<T>>(key.initial());
        }));
    return StateHandle<T>{std::move(slot), anchor()};
  }

  template <class T>
  auto state(const StateKey<T>& key, std::type_identity_t<T> initial)
      -> StateHandle<T> {
    auto slot = std::static_pointer_cast<StateSlot::Typed<T>>(
        slot_for(key.identity(), [&initial] {
          return std::make_shared<StateSlot::Typed<T>>(std::move(initial));
        }));
    return StateHandle<T>{std::move(slot), anchor()};
  }

  template <class T>
  auto state(const LazyState<T>& key) -> StateHandle<T> {
    auto slot = std::static_pointer_cast<StateSlot::Typed<T>>(
        slot_for(key.identity(), [&key] {
          return std::make_shared<StateSlot::Typed<T>>(key.initial_value());
        }));
    return StateHandle<T>{std::move(slot), anchor()};
  }

  template <class T>
  auto state(const FocusState<T>& key) -> FocusBinding<T> {
    return FocusBinding<T>{state(key.key())};
  }

  template <class T>
  auto state(const AccessibilityFocusState<T>& key)
      -> AccessibilityFocusBinding<T> {
    const AccessibilityTechnologies running =
        environment(AccessibilitySettings::technologies);
    return AccessibilityFocusBinding<T>{
        state(key.key()), !running.intersection(key.technologies()).empty()};
  }

  template <class T>
  auto state(const GestureState<T>& key) -> GestureStateHandle<T> {
    return GestureStateHandle<T>{state(key.key()), key};
  }

  auto scroll_view_proxy() const -> ScrollViewProxy;

  template <class T>
  auto retained(const StateKey<T>& key) -> T& {
    auto slot = std::static_pointer_cast<StateSlot::Typed<T>>(
        slot_for(key.identity(), [&key] {
          return std::make_shared<StateSlot::Typed<T>>(key.initial());
        }));
    return slot->value();
  }

  auto namespace_id(const Namespace& space) const -> Namespace::ID;

  auto frame_time() const -> std::chrono::nanoseconds;

  auto transaction() const -> const motion::Transaction&;

  auto motion_moment() -> motion::Presentation::Moment;

  auto node_anchor() const -> std::weak_ptr<const NodeAnchor>;

  void request_frame();

  void select_link(std::size_t link);

  template <class T>
  auto read(const Observable<T>& observable) -> const T& {
    observe(*observable.core_);
    return observable.value_;
  }

  template <class T, class Field>
  auto read(const Observable<T>& observable, Field T::* member)
      -> const Field& {
    observe(observable.core_->channel(Observable<T>::channel_of(member)));
    return observable.value_.*member;
  }

  template <class T>
  auto environment_chain(const EnvironmentKey<T>& key)
      -> std::vector<std::reference_wrapper<const T>> {
    std::vector<std::reference_wrapper<const T>> found;
    for (const EnvironmentOverride::Value& value : lookup_all(key.identity())) {
      found.push_back(std::cref(
          static_cast<const EnvironmentOverride::Typed<T>&>(value).get()));
    }
    return found;
  }

  template <class T>
  auto environment(const EnvironmentKey<T>& key) -> const T& {
    const auto found = lookup(key.identity());
    if (!found.has_value()) {
      return key.fallback();
    }
    return static_cast<const EnvironmentOverride::Typed<T>&>(found->get())
        .get();
  }

  auto environment_values() -> EnvironmentValues;

  auto environment_snapshot() const -> EnvironmentValues;

  template <class T>
  auto environment_object()
      -> std::optional<std::reference_wrapper<Observable<T>>> {
    const std::optional<EnvironmentObject<T>>& held =
        environment(environment_object_key<T>);
    if (!held.has_value()) {
      return std::nullopt;
    }
    return std::ref(held->get());
  }

  template <class T>
  auto app_storage(const AppStorage<T>& key) -> Binding<T> {
    if (const auto store = key.store(); store.has_value()) {
      return stored(std::optional<UserDefaults>{store->get()}, key.name(),
                    key.local());
    }
    return stored(environment(UserDefaults::app_storage), key.name(),
                  key.local());
  }

  template <class T>
  auto app_storage(std::string_view name, const StateKey<T>& local)
      -> Binding<T> {
    return stored(environment(UserDefaults::app_storage), name, local);
  }

  template <class T>
  auto scene_storage(const SceneStorage<T>& key) -> Binding<T> {
    if (const auto store = key.store(); store.has_value()) {
      return stored(std::optional<UserDefaults>{store->get()}, key.name(),
                    key.local());
    }
    return stored(environment(UserDefaults::scene_storage), key.name(),
                  key.local());
  }

  auto is_focused() -> bool;

  auto is_pressed() -> bool;

  auto focus_within() -> bool;

  auto hover_within() -> bool;

  template <class T>
  auto focused_value(const FocusedValueKey<T>& key) -> std::optional<T> {
    const EnvironmentOverride::Value* found = focused_lookup(key.identity());
    if (found == nullptr) {
      return std::nullopt;
    }
    return FocusedValueKey<T>::read(*found);
  }

  template <class T>
  auto focused_binding(const FocusedValueKey<Binding<T>>& key)
      -> FocusedBinding<T> {
    return FocusedBinding<T>{focused_value(key)};
  }

  auto focused_values() -> FocusedValues;

  template <class T>
  auto focused_object()
      -> std::optional<std::reference_wrapper<Observable<T>>> {
    const std::optional<FocusedObject<T>> held =
        focused_value(focused_object_key<T>);
    if (!held.has_value()) {
      return std::nullopt;
    }
    return std::ref(held->get());
  }

  auto services() const -> const RootServices&;

  void fail(UiError error);

 private:
  friend class Reconciler;
  friend class Tree;

  using SlotFactory = std::move_only_function<std::shared_ptr<StateSlot>()>;

  Context(Tree& tree, Node& node) noexcept : tree_(tree), node_(node) {}

  auto slot_for(Identity key, SlotFactory create) -> std::shared_ptr<StateSlot>;

  auto anchor() const -> std::weak_ptr<const NodeAnchor>;

  void observe(ObservableCore& core);

  template <class T>
  auto stored(std::optional<UserDefaults> store, std::string_view name,
              const StateKey<T>& local) -> Binding<T> {
    if (!store.has_value()) {
      return Binding<T>{state(local)};
    }
    observe(store->values().core_->channel(UserDefaults::channel(name)));
    if constexpr (StoredOptional<T>) {
      using Held = typename T::value_type;
      return Binding<T>{[held = *store, name = std::string{name},
                         fallback = local.initial()] {
                          std::optional<Held> found = held.get<Held>(name);
                          return found.has_value() ? T{std::move(*found)}
                                                   : fallback;
                        },
                        [held = *store, name = std::string{name}](T value) {
                          if (value.has_value()) {
                            held.put(name, *value);
                          } else {
                            held.remove(name);
                          }
                        }};
    } else {
      return Binding<T>{[held = *store, name = std::string{name},
                         fallback = local.initial()] {
                          return held.get<T>(name).value_or(fallback);
                        },
                        [held = *store, name = std::string{name}](T value) {
                          held.put(name, value);
                        }};
    }
  }

  auto focus_flag(const Node& reader)
      -> std::reference_wrapper<const EnvironmentOverride::Value>;

  auto focused_lookup(Identity key) -> const EnvironmentOverride::Value*;

  auto lookup(Identity key) -> std::optional<
      std::reference_wrapper<const EnvironmentOverride::Value>>;

  auto lookup_all(Identity key)
      -> std::vector<std::reference_wrapper<const EnvironmentOverride::Value>>;

  std::reference_wrapper<Tree> tree_;
  std::reference_wrapper<Node> node_;
};

}
