#include <arisa/composition/list_environment.hpp>

namespace arisa::composition {

const EnvironmentKey<Prominence> ListEnvironment::header_prominence{
    "header_prominence", Prominence::standard};
const EnvironmentKey<std::optional<float>> ListEnvironment::row_spacing{
    "list_row_spacing", std::nullopt};
const EnvironmentKey<std::optional<float>> ListEnvironment::section_spacing{
    "list_section_spacing", std::nullopt};
const EnvironmentKey<std::optional<float>> ListEnvironment::card_corner_radius{
    "list_card_corner_radius", std::nullopt};
const EnvironmentKey<std::optional<float>>
    ListEnvironment::default_min_row_height{"default_min_list_row_height",
                                            std::nullopt};
const EnvironmentKey<std::optional<float>>
    ListEnvironment::default_min_header_height{"default_min_list_header_height",
                                               std::nullopt};
const EnvironmentKey<bool> ListEnvironment::alternating_row_backgrounds{
    "alternating_row_backgrounds", false};

const EnvironmentKey<AlternatingRowBackgroundBehavior>
    ListEnvironment::alternating_row_background_behavior{
        "alternating_row_background_behavior",
        AlternatingRowBackgroundBehavior::automatic};
const EnvironmentKey<Visibility> ListEnvironment::section_index_visibility{
    "list_section_index_visibility", Visibility::automatic};
const EnvironmentKey<BadgeProminence> ListEnvironment::badge_prominence{
    "badge_prominence", BadgeProminence::standard};
const EnvironmentKey<EdgeLengths> ListEnvironment::section_margins{
    "list_section_margins", EdgeLengths{}};
const EnvironmentKey<ListRow::Separator> ListEnvironment::section_separator_top{
    "list_section_separator_top", ListRow::Separator{}};
const EnvironmentKey<ListRow::Separator>
    ListEnvironment::section_separator_bottom{"list_section_separator_bottom",
                                              ListRow::Separator{}};
const EnvironmentKey<ListEnvironment::SectionActions>
    ListEnvironment::section_actions{"section_actions", nullptr};
const EnvironmentKey<SidebarRowSize> ListEnvironment::sidebar_row_size{
    "sidebar_row_size", SidebarRowSize::medium};
const EnvironmentKey<bool> ListEnvironment::swipe_actions_container{
    "swipe_actions_container", false};
const EnvironmentKey<const SwipeHost*> ListEnvironment::swipe_host{"swipe_host",
                                                                   nullptr};
const EnvironmentKey<Visibility> ListEnvironment::table_column_headers{
    "table_column_headers", Visibility::automatic};

}
