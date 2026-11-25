#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/tag_key.hpp>
#include <arisa/structure/detail_presenter.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::structure {

class SidebarSelection {
 public:
  class Reader;

  using Current = std::function<composition::TagKey()>;

  static const composition::EnvironmentKey<std::optional<SidebarSelection>> key;

  SidebarSelection(composition::FocusBinding<composition::TagKey> focus,
                   DetailPresenter::Commit commit,
                   std::shared_ptr<Reader> reader) noexcept
      : focus_(std::move(focus)),
        commit_(std::move(commit)),
        reader_(std::move(reader)) {}

  auto focus() const noexcept
      -> const composition::FocusBinding<composition::TagKey>& {
    return focus_;
  }

  void present(composition::TagKey chosen) const;

  void read(Current current) const;

  friend auto operator==(const SidebarSelection& left,
                         const SidebarSelection& right) -> bool {
    return left.focus_.identity() == right.focus_.identity() &&
           left.commit_ == right.commit_ && left.reader_ == right.reader_;
  }

 private:
  composition::FocusBinding<composition::TagKey> focus_;
  DetailPresenter::Commit commit_;
  std::shared_ptr<Reader> reader_;
};

class SidebarSelection::Reader {
 public:
  void read(Current current) { current_ = std::move(current); }

  auto current() const -> std::optional<composition::TagKey>;

 private:
  Current current_;
};

}
