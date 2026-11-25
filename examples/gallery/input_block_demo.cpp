#include <input_block_demo.hpp>

#include <arisa/async/awaitables.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

auto InputBlockDemo::expire(composition::StateHandle<bool> holding,
                            std::shared_ptr<InputBlocker> blocker)
    -> async::Task<void> {
  co_await async::sleep(duration);
  blocker->release();
  holding.set(false);
}

auto InputBlockDemo::body(composition::Context& context) const
    -> composition::Element {
  const auto holding = context.state(held);
  const std::shared_ptr<InputBlocker> blocker =
      context.environment(InputBlocker::key);
  std::vector<composition::Element> rows;
  rows.push_back(components::Button("Block input",
                                    [holding, blocker] {
                                      if (blocker == nullptr) {
                                        return;
                                      }
                                      blocker->hold();
                                      holding.set(blocker->holding());
                                    })
                     .disabled(blocker == nullptr)
                     .id(std::string{block_button}));
  if (holding.get()) {
    rows.push_back(
        components::HStack(
            components::HStackOptions{
                .alignment = components::VerticalAlignment::center,
                .spacing = theme::space::sm},
            controls::ProgressView(),
            components::Text("Blocked")
                .font(theme::Font::footnote)
                .foreground_style(theme::ColourRole::text_secondary))
            .task([holding, blocker] { return expire(holding, blocker); }));
  }
  composition::Element column =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::sm},
          std::move(rows))
          .frame({.max_width = composition::infinity,
                  .alignment = composition::Alignment::leading});
  if (!holding.get() || blocker == nullptr) {
    return column;
  }
  return std::move(column).on_command(provider::Button::minus, "Release",
                                      [holding, blocker] {
                                        blocker->release();
                                        holding.set(false);
                                      });
}

}
