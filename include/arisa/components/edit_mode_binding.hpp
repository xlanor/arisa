#pragma once

#include <arisa/components/edit_mode.hpp>
#include <arisa/composition/binding.hpp>

#include <utility>

namespace arisa::components {

class EditModeBinding {
 public:
  explicit EditModeBinding(composition::Binding<EditMode> mode) noexcept
      : mode_(std::move(mode)) {}

  auto get() const -> EditMode { return mode_.get(); }

  void set(EditMode mode) const { mode_.set(mode); }

  auto is_editing() const -> bool { return get() != EditMode::inactive; }

  void toggle() const {
    set(is_editing() ? EditMode::inactive : EditMode::active);
  }

  friend auto operator==(const EditModeBinding& left,
                         const EditModeBinding& right) -> bool {
    return left.mode_.identity() == right.mode_.identity();
  }

 private:
  composition::Binding<EditMode> mode_;
};

}
