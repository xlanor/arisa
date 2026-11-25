#pragma once

#include <arisa/composition/observable.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::composition {

class UndoManager {
 public:
  using Action = std::function<void(const UndoManager&)>;

  struct Status {
    bool can_undo{false};
    bool can_redo{false};
    std::string undo_action_name{};
    std::string redo_action_name{};

    friend auto operator==(const Status&, const Status&) -> bool = default;
  };

  UndoManager();

  void register_undo(Action action) const;

  void register_undo(std::string name, Action action) const;

  void set_action_name(std::string name) const;

  void begin_undo_grouping() const;

  void end_undo_grouping() const;

  auto grouping_level() const noexcept -> std::size_t;

  void undo() const;

  void redo() const;

  auto can_undo() const noexcept -> bool;

  auto can_redo() const noexcept -> bool;

  auto is_undoing() const noexcept -> bool;

  auto is_redoing() const noexcept -> bool;

  auto undo_action_name() const -> std::string;

  auto redo_action_name() const -> std::string;

  auto levels_of_undo() const noexcept -> std::size_t;

  void set_levels_of_undo(std::size_t levels) const;

  void remove_all_actions() const;

  auto status() const noexcept -> const Observable<Status>&;

  friend auto operator==(const UndoManager& left, const UndoManager& right)
      -> bool {
    return left.state_ == right.state_;
  }

 private:
  struct Group {
    std::string name;
    std::vector<Action> actions;
  };

  struct State;

  void run(std::vector<Group>& from, bool undoing) const;

  void publish() const;

  void trim() const;

  std::shared_ptr<State> state_;
};

}
