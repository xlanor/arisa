#include <arisa/composition/undo_manager.hpp>

#include <ranges>
#include <utility>

namespace arisa::composition {

struct UndoManager::State {
  std::vector<Group> undo;
  std::vector<Group> redo;
  std::optional<Group> open;
  std::size_t level{0};
  std::size_t levels{0};
  bool undoing{false};
  bool redoing{false};
  Observable<Status> status;
};

UndoManager::UndoManager() : state_(std::make_shared<State>()) {}

void UndoManager::register_undo(Action action) const {
  register_undo(std::string{}, std::move(action));
}

void UndoManager::register_undo(std::string name, Action action) const {
  State& state = *state_;
  if (state.open.has_value()) {
    state.open->actions.push_back(std::move(action));
    if (!name.empty()) {
      state.open->name = std::move(name);
    }
    return;
  }
  Group group{.name = std::move(name), .actions = {}};
  group.actions.push_back(std::move(action));
  if (state.undoing) {
    state.redo.push_back(std::move(group));
  } else {
    state.undo.push_back(std::move(group));
    if (!state.redoing) {
      state.redo.clear();
    }
    trim();
  }
  publish();
}

void UndoManager::set_action_name(std::string name) const {
  State& state = *state_;
  if (state.open.has_value()) {
    state.open->name = std::move(name);
    return;
  }
  std::vector<Group>& target = state.undoing ? state.redo : state.undo;
  if (!target.empty()) {
    target.back().name = std::move(name);
    publish();
  }
}

void UndoManager::begin_undo_grouping() const {
  State& state = *state_;
  if (state.level == 0) {
    state.open.emplace();
  }
  ++state.level;
}

void UndoManager::end_undo_grouping() const {
  State& state = *state_;
  if (state.level == 0) {
    return;
  }
  --state.level;
  if (state.level > 0 || !state.open.has_value()) {
    return;
  }
  Group group = std::move(*state.open);
  state.open.reset();
  if (group.actions.empty()) {
    return;
  }
  if (state.undoing) {
    state.redo.push_back(std::move(group));
  } else {
    state.undo.push_back(std::move(group));
    if (!state.redoing) {
      state.redo.clear();
    }
    trim();
  }
  publish();
}

auto UndoManager::grouping_level() const noexcept -> std::size_t {
  return state_->level;
}

void UndoManager::run(std::vector<Group>& from, bool undoing) const {
  State& state = *state_;
  if (from.empty() || state.level > 0) {
    return;
  }
  Group group = std::move(from.back());
  from.pop_back();
  (undoing ? state.undoing : state.redoing) = true;
  begin_undo_grouping();
  for (Action& action : group.actions | std::views::reverse) {
    if (action) {
      action(*this);
    }
  }
  if (state.open.has_value() && !group.name.empty()) {
    state.open->name = group.name;
  }
  end_undo_grouping();
  (undoing ? state.undoing : state.redoing) = false;
  publish();
}

void UndoManager::undo() const { run(state_->undo, true); }

void UndoManager::redo() const { run(state_->redo, false); }

auto UndoManager::can_undo() const noexcept -> bool {
  return !state_->undo.empty();
}

auto UndoManager::can_redo() const noexcept -> bool {
  return !state_->redo.empty();
}

auto UndoManager::is_undoing() const noexcept -> bool {
  return state_->undoing;
}

auto UndoManager::is_redoing() const noexcept -> bool {
  return state_->redoing;
}

auto UndoManager::undo_action_name() const -> std::string {
  return state_->undo.empty() ? std::string{} : state_->undo.back().name;
}

auto UndoManager::redo_action_name() const -> std::string {
  return state_->redo.empty() ? std::string{} : state_->redo.back().name;
}

auto UndoManager::levels_of_undo() const noexcept -> std::size_t {
  return state_->levels;
}

void UndoManager::set_levels_of_undo(std::size_t levels) const {
  state_->levels = levels;
  trim();
  publish();
}

void UndoManager::remove_all_actions() const {
  State& state = *state_;
  state.undo.clear();
  state.redo.clear();
  state.open.reset();
  state.level = 0;
  publish();
}

auto UndoManager::status() const noexcept -> const Observable<Status>& {
  return state_->status;
}

void UndoManager::trim() const {
  State& state = *state_;
  if (state.levels == 0 || state.undo.size() <= state.levels) {
    return;
  }
  state.undo.erase(state.undo.begin(),
                   state.undo.begin() + static_cast<std::ptrdiff_t>(
                                            state.undo.size() - state.levels));
}

void UndoManager::publish() const {
  State& state = *state_;
  state.status.set(Status{.can_undo = !state.undo.empty(),
                          .can_redo = !state.redo.empty(),
                          .undo_action_name = undo_action_name(),
                          .redo_action_name = redo_action_name()});
}

}
