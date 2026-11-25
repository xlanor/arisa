#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/structure/document_launch_view.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace arisa::structure {

class DocumentGroupLaunchScene {
 public:
  using Build = DocumentLaunchView::Build;
  using Options = DocumentLaunchView::Options;

  static const composition::EnvironmentKey<
      std::optional<DocumentGroupLaunchScene>>
      key;

  DocumentGroupLaunchScene(std::string title, Build actions,
                           Options options = {});

  DocumentGroupLaunchScene(std::string title,
                           graphics::ShapeStyle background_style,
                           Build actions = {}, Options options = {});

  auto title() const noexcept -> const std::string& { return state_->title; }

  auto actions() const noexcept -> const Build& { return state_->actions; }

  auto options() const noexcept -> const Options& { return state_->options; }

  friend auto operator==(const DocumentGroupLaunchScene&,
                         const DocumentGroupLaunchScene&) -> bool = default;

 private:
  struct State {
    std::string title;
    Build actions;
    Options options;
  };

  std::shared_ptr<const State> state_;
};

}

namespace arisa::composition {

template <class Scene>
auto Element::document_group_launch_scene(Scene scene) && -> Element {
  return std::move(*this).environment(
      structure::DocumentGroupLaunchScene::key,
      std::optional<structure::DocumentGroupLaunchScene>{std::move(scene)});
}

}
