#pragma once

#include <arisa/composition/drop_operation.hpp>

namespace arisa::composition {

class DragConfiguration {
 public:
  class OperationsWithinApp {
   public:
    constexpr OperationsWithinApp() noexcept
        : copy_(true), move_(false), delete_(false), alias_(false) {}

    constexpr explicit OperationsWithinApp(bool allow_move) noexcept
        : copy_(true), move_(allow_move), delete_(false), alias_(false) {}

    constexpr OperationsWithinApp(bool allow_copy, bool allow_move,
                                  bool allow_delete = false) noexcept
        : copy_(allow_copy),
          move_(allow_move),
          delete_(allow_delete),
          alias_(false) {}

    constexpr auto allow_alias() const noexcept -> bool { return alias_; }

    constexpr void set_allow_alias(bool allow) noexcept { alias_ = allow; }

    constexpr auto allowed() const noexcept -> DropOperationSet {
      return permitted(copy_, move_, delete_, alias_);
    }

    friend constexpr auto operator==(const OperationsWithinApp&,
                                     const OperationsWithinApp&)
        -> bool = default;

   private:
    bool copy_;
    bool move_;
    bool delete_;
    bool alias_;
  };

  class OperationsOutsideApp {
   public:
    constexpr OperationsOutsideApp() noexcept
        : copy_(true), move_(false), delete_(false), alias_(false) {}

    constexpr explicit OperationsOutsideApp(bool allow_copy) noexcept
        : copy_(allow_copy), move_(false), delete_(false), alias_(false) {}

    constexpr OperationsOutsideApp(bool allow_copy, bool allow_move,
                                   bool allow_delete = false) noexcept
        : copy_(allow_copy),
          move_(allow_move),
          delete_(allow_delete),
          alias_(false) {}

    constexpr auto allow_alias() const noexcept -> bool { return alias_; }

    constexpr void set_allow_alias(bool allow) noexcept { alias_ = allow; }

    constexpr auto allowed() const noexcept -> DropOperationSet {
      return permitted(copy_, move_, delete_, alias_);
    }

    friend constexpr auto operator==(const OperationsOutsideApp&,
                                     const OperationsOutsideApp&)
        -> bool = default;

   private:
    bool copy_;
    bool move_;
    bool delete_;
    bool alias_;
  };

  constexpr explicit DragConfiguration(
      OperationsWithinApp within = OperationsWithinApp{},
      OperationsOutsideApp outside = OperationsOutsideApp{}) noexcept
      : within_(within), outside_(outside) {}

  constexpr explicit DragConfiguration(bool allow_move) noexcept
      : within_(allow_move) {}

  constexpr DragConfiguration(bool allow_move, bool allow_delete) noexcept
      : within_(true, allow_move, allow_delete) {}

  constexpr auto operations_within_app() const noexcept
      -> const OperationsWithinApp& {
    return within_;
  }

  constexpr void set_operations_within_app(
      OperationsWithinApp within) noexcept {
    within_ = within;
  }

  constexpr auto operations_outside_app() const noexcept
      -> const OperationsOutsideApp& {
    return outside_;
  }

  constexpr void set_operations_outside_app(
      OperationsOutsideApp outside) noexcept {
    outside_ = outside;
  }

  constexpr auto permits(DropOperation operation) const noexcept -> bool {
    return operation == DropOperation::cancel ||
           within_.allowed().contains(operation);
  }

  friend constexpr auto operator==(const DragConfiguration&,
                                   const DragConfiguration&) -> bool = default;

 private:
  static constexpr auto permitted(bool copy, bool move, bool remove,
                                  bool alias) noexcept -> DropOperationSet {
    DropOperationSet allowed;
    if (copy) {
      allowed = allowed | DropOperationSet::copy;
    }
    if (move) {
      allowed = allowed | DropOperationSet::move;
    }
    if (remove) {
      allowed = allowed | DropOperationSet::remove;
    }
    if (alias) {
      allowed = allowed | DropOperationSet::alias;
    }
    return allowed;
  }

  OperationsWithinApp within_;
  OperationsOutsideApp outside_;
};

}
