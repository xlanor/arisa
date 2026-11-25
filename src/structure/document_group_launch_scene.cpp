#include <arisa/structure/document_group_launch_scene.hpp>

#include <arisa/structure/default_document_group_launch_actions.hpp>

#include <utility>

namespace arisa::structure {

const composition::EnvironmentKey<std::optional<DocumentGroupLaunchScene>>
    DocumentGroupLaunchScene::key{"arisa.document-group-launch-scene",
                                  std::nullopt};

DocumentGroupLaunchScene::DocumentGroupLaunchScene(std::string title,
                                                   Build actions,
                                                   Options options)
    : state_(std::make_shared<const State>(State{
          .title = std::move(title),
          .actions = actions ? std::move(actions) : Build{[] {
            return composition::Element{DefaultDocumentGroupLaunchActions{}};
          }},
          .options = std::move(options)})) {}

DocumentGroupLaunchScene::DocumentGroupLaunchScene(
    std::string title, graphics::ShapeStyle background_style, Build actions,
    Options options)
    : DocumentGroupLaunchScene(
          std::move(title), std::move(actions), [&options, &background_style] {
            Options styled = std::move(options);
            styled.background_style = std::move(background_style);
            return styled;
          }()) {}

}
