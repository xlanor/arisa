#include <arisa/composition/search_environment.hpp>

namespace arisa::composition {

const EnvironmentKey<bool> SearchEnvironment::is_searching{"is_searching",
                                                           false};
const EnvironmentKey<SearchEnvironment::Activation>
    SearchEnvironment::activation{"search_activation",
                                  SearchEnvironment::Activation{}};
const EnvironmentKey<SearchEnvironment::SuggestionVisibility>
    SearchEnvironment::suggestion_visibility{
        "search_suggestion_visibility", SuggestionVisibility{},
        &SearchEnvironment::SuggestionVisibility::merged};
const EnvironmentKey<SearchSuggestionsPlacement>
    SearchEnvironment::suggestions_placement{
        "search_suggestions_placement", SearchSuggestionsPlacement::automatic};
const EnvironmentKey<SearchPresentationToolbarBehavior>
    SearchEnvironment::presentation_toolbar_behavior{
        "search_presentation_toolbar_behavior",
        SearchPresentationToolbarBehavior::automatic};
const EnvironmentKey<SearchToolbarBehavior> SearchEnvironment::toolbar_behavior{
    "search_toolbar_behavior", SearchToolbarBehavior::automatic};
const EnvironmentKey<std::shared_ptr<const SearchEnvironment::Builder>>
    SearchEnvironment::sidebar_field{"search_sidebar_field", nullptr};
const EnvironmentKey<DismissSearchAction> SearchEnvironment::dismiss_search{
    "dismiss_search", DismissSearchAction{}};
const EnvironmentKey<std::shared_ptr<const FocusBinding<bool>>>
    SearchEnvironment::focused{"search_focused", nullptr};
const EnvironmentKey<std::shared_ptr<const SearchEnvironment::FocusTarget>>
    SearchEnvironment::focus_target{"search_focus_target", nullptr};
const EnvironmentKey<std::shared_ptr<const SearchEnvironment::Builder>>
    SearchEnvironment::suggestions{"search_suggestions", nullptr};
const EnvironmentKey<std::shared_ptr<const SearchEnvironment::Scopes>>
    SearchEnvironment::scopes{"search_scopes", nullptr};
const EnvironmentKey<SearchEnvironment::Completion>
    SearchEnvironment::completion{"search_completion",
                                  SearchEnvironment::Completion{}};

const EnvironmentKey<
    std::shared_ptr<const Binding<std::optional<TextSelection>>>>
    SearchEnvironment::selection{"search_selection", nullptr};

}
