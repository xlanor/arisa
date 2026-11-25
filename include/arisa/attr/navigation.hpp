#pragma once

#include <arisa/attr/presentation.hpp>
#include <arisa/components/safe_area_inset.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/navigation_split_view_style.hpp>
#include <arisa/composition/toolbar_placement.hpp>
#include <arisa/composition/toolbar_role.hpp>
#include <arisa/composition/toolbar_title_display_mode.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/presentation/presented_destination.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour_value.hpp>

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace arisa::attr {

class NavigationSubtitle : public Presented {
 public:
  explicit NavigationSubtitle(std::string subtitle)
      : Presented(
            [held = std::move(subtitle)](composition::Element element) mutable {
              return std::move(element).navigation_subtitle(std::move(held));
            }) {}
};

class NavigationBarBackButtonHidden : public Presented {
 public:
  explicit NavigationBarBackButtonHidden(bool hides = true)
      : Presented([hides](composition::Element element) {
          return std::move(element).navigation_bar_back_button_hidden(hides);
        }) {}
};

class ToolbarTitleDisplayMode : public Presented {
 public:
  explicit ToolbarTitleDisplayMode(composition::ToolbarTitleDisplayMode mode)
      : Presented([mode](composition::Element element) {
          return std::move(element).toolbar_title_display_mode(mode);
        }) {}
};

class ToolbarRole : public Presented {
 public:
  explicit ToolbarRole(composition::ToolbarRole chosen)
      : Presented([chosen](composition::Element element) {
          return std::move(element).toolbar_role(chosen);
        }) {}
};

class ToolbarVisibility : public Presented {
 public:
  explicit ToolbarVisibility(composition::Visibility visibility,
                             composition::ToolbarPlacement bars =
                                 composition::ToolbarPlacement::automatic)
      : Presented([visibility, bars](composition::Element element) {
          return std::move(element).toolbar_visibility(visibility, bars);
        }) {}
};

class ToolbarBackground : public Presented {
 public:
  explicit ToolbarBackground(theme::ColourValue style,
                             composition::ToolbarPlacement bars =
                                 composition::ToolbarPlacement::automatic)
      : Presented([style, bars](composition::Element element) {
          return std::move(element).toolbar_background(style, bars);
        }) {}

  explicit ToolbarBackground(composition::Visibility visibility,
                             composition::ToolbarPlacement bars =
                                 composition::ToolbarPlacement::automatic)
      : Presented([visibility, bars](composition::Element element) {
          return std::move(element).toolbar_background(visibility, bars);
        }) {}
};

class ToolbarColorScheme : public Presented {
 public:
  explicit ToolbarColorScheme(std::optional<theme::ColorScheme> scheme,
                              composition::ToolbarPlacement bars =
                                  composition::ToolbarPlacement::automatic)
      : Presented([scheme, bars](composition::Element element) {
          return std::move(element).toolbar_color_scheme(scheme, bars);
        }) {}
};

class ToolbarForegroundStyle : public Presented {
 public:
  explicit ToolbarForegroundStyle(theme::ColourValue style,
                                  composition::ToolbarPlacement bars =
                                      composition::ToolbarPlacement::automatic)
      : Presented([style, bars](composition::Element element) {
          return std::move(element).toolbar_foreground_style(style, bars);
        }) {}
};

class ToolbarTitleMenu : public Presented {
 public:
  explicit ToolbarTitleMenu(std::function<composition::Element()> content)
      : Presented(
            [held = std::move(content)](composition::Element element) mutable {
              return std::move(element).toolbar_title_menu(std::move(held));
            }) {}
};

class NavigationSplitViewColumnWidth : public Presented {
 public:
  explicit NavigationSplitViewColumnWidth(float width)
      : Presented([width](composition::Element element) {
          return std::move(element).navigation_split_view_column_width(width);
        }) {}

  NavigationSplitViewColumnWidth(std::optional<float> minimum, float ideal,
                                 std::optional<float> maximum)
      : Presented([minimum, ideal, maximum](composition::Element element) {
          return std::move(element).navigation_split_view_column_width(
              minimum, ideal, maximum);
        }) {}
};

class NavigationSplitViewStyleAttribute : public Presented {
 public:
  explicit NavigationSplitViewStyleAttribute(
      composition::NavigationSplitViewStyle style)
      : Presented([style](composition::Element element) {
          return std::move(element).navigation_split_view_style(style);
        }) {}
};

class SafeAreaInset : public Presented {
 public:
  template <class... Arguments>
    requires(sizeof...(Arguments) >= 2)
  explicit SafeAreaInset(Arguments... arguments)
      : Presented([... held = std::move(arguments)](
                      composition::Element element) mutable {
          return std::move(element).safe_area_inset(std::move(held)...);
        }) {}
};

class SafeAreaBar : public Presented {
 public:
  template <class... Arguments>
    requires(sizeof...(Arguments) >= 1)
  explicit SafeAreaBar(Arguments... arguments)
      : Presented([... held = std::move(arguments)](
                      composition::Element element) mutable {
          return std::move(element).safe_area_bar(std::move(held)...);
        }) {}
};

class TabViewSidebarHeader : public Presented {
 public:
  template <class Content>
  explicit TabViewSidebarHeader(Content content)
      : Presented([held = std::move(content)](
                      composition::Element element) mutable {
          return std::move(element).tab_view_sidebar_header(std::move(held));
        }) {}
};

class TabViewSidebarFooter : public Presented {
 public:
  template <class Content>
  explicit TabViewSidebarFooter(Content content)
      : Presented([held = std::move(content)](
                      composition::Element element) mutable {
          return std::move(element).tab_view_sidebar_footer(std::move(held));
        }) {}
};

using navigation_subtitle = NavigationSubtitle;
using navigation_bar_back_button_hidden = NavigationBarBackButtonHidden;
using navigation_bar_title_display_mode = ToolbarTitleDisplayMode;
using toolbar_title_display_mode = ToolbarTitleDisplayMode;
using toolbar_role = ToolbarRole;
using toolbar_visibility = ToolbarVisibility;
using toolbar_background = ToolbarBackground;
using toolbar_color_scheme = ToolbarColorScheme;
using toolbar_foreground_style = ToolbarForegroundStyle;
using toolbar_title_menu = ToolbarTitleMenu;
using navigation_split_view_column_width = NavigationSplitViewColumnWidth;
using navigation_split_view_style = NavigationSplitViewStyleAttribute;
using safe_area_inset = SafeAreaInset;
using safe_area_bar = SafeAreaBar;
using tab_view_sidebar_header = TabViewSidebarHeader;
using tab_view_sidebar_footer = TabViewSidebarFooter;

}
