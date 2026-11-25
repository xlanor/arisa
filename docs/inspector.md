# Inspector

The inspector is a debug overlay built from arisa's own components. It draws through the same painter as the app, so it runs unchanged on deko3d (Switch 1), Vulkan (Switch 2), SDL_GPU and the headless NullGpu, and it never calls a GPU backend API.

## Opting in

The inspector is its own library, `arisa_inspector`. It is built when the `ARISA_INSPECTOR` CMake option is on: on by default for host builds, off by default for Switch Release builds unless the preset sets it (the `nx` and `ounce` presets do, so the example NROs carry it). An app that does not link `arisa_inspector` contains none of it. Linking it defines `ARISA_INSPECTOR=1` for the app.

```cmake
if(TARGET arisa_inspector)
    target_link_libraries(my_app PRIVATE arisa_inspector)
endif()
```

```cpp
#ifdef ARISA_INSPECTOR
#include <arisa/inspector/inspector.hpp>
#endif

auto application = arisa::runtime::Application::create(providers, content, options);
#ifdef ARISA_INSPECTOR
arisa::inspector::Inspector inspector = arisa::inspector::Inspector::attach(*application);
#endif
```

`Inspector::attach(Application&, InspectorOptions = {})` registers a `runtime::FrameObserver` with the application and returns a move-only handle; destroying it unregisters the observer. Keep it declared after the application so it goes first. `InspectorOptions` sets the chord (`chord`, default L+R+minus), the desktop key (`key`, default F12), the history length (`history`, 300), the number of ranked nodes (`ranked`, 10), the panel width (`panel_width`, 560 points, capped at half the viewport) and where the Tokens panel exports (`exports`, a `StorageProvider`; by default the appearance model's theme folder or user storage).

## Opening

Press the chord on a pad or the key on a keyboard, or call `open()`, `close()` or `toggle()`. The chord buttons are removed from the frame the app sees while the chord is held, and the key event is removed too.

The inspector is a modal layer at `OverlayLayer::debug`, added at the root of the app's tree with `Tree::present_debug_layer`. While it is open it holds focus and input; the app receives nothing. When it closes, the layer leaves and focus returns to the node that had it. The Focus panel shows the app's focus as it was when the inspector opened.

## Panels

The header is one row: the panel tabs, then Pause and Close as icon buttons whose labels stay their accessible names.

- **Tree**: the node tree with component names and keys, collapsed below depth 3. Rows indent relative to the shallowest visible row, with the step shrunk so the deepest visible row keeps 60 % of the panel width for its label; labels are one line and end in an ellipsis when they do not fit (`Text::line_limit(1)`). Selecting a row outlines the node's bounds over the app and shows its frame, bounds, padding, flags (focusable, clip, glass, layer-promoted, overlay, animating) and its cumulative evaluations, layout passes, recordings and shaping runs.
- **Pick**: a cursor over the app. Touch picks the node under the finger; the d-pad moves the cursor, A picks and B goes back to Tree. Touches on the panel itself still work the panel. A pick selects the deepest node under the point and switches to Tree.
- **Focus**: the focused node, its chain from the root, the focus scopes and sections on that chain, and the node focus would move to in each direction.
- **Timeline**: the last 300 frames as stacked bars of evaluate, layout, input, record, upload, emit and present on a fixed 33.3 ms scale (twice the frame budget, with a 16.7 ms line); taller frames are clipped and get an orange cap tick. GPU effects, UI and hooks sit underneath on their own 4.2 ms scale (`TimelineScale`), and frames with missed vsyncs get a red mark (deko3d and Vulkan count them; the host backends do not). A one-off spike never rescales the other bars. Tap a bar or use Previous and Next to show that frame's `FrameBreakdown::describe` line and its counters.
- **Cost**: the nodes that cost most, ranked by evaluations + layout passes + recordings + shaping runs, either over the window since the inspector opened or for the frame chosen in the timeline.
- **GPU**: backend and startup line, texture and render-target memory, glyph atlas, the render-target pool, external textures with their publication state, frame hooks with their drive, and surfaces with their mode and drive. arisa has no per-Root GPU budget yet, so the panel shows totals.
- **Tokens**: the active theme's tokens, grouped as the theme file groups them (Colours, Metrics, Radii, Spacing, Type, Materials, Liquid glass, Glass, Motion). Group headers expand one group at a time; Colours shows the roles for the scheme on screen. Each row shows the token, its value and, once edited, an Undo button that puts that token back. Selecting a row opens its editor below it, scrolled into view: colours get a hex field and hue, saturation, brightness and alpha sliders; numbers a slider over the token's range and step; toggles a switch; choices a row of buttons. Every control works with the d-pad (up and down between controls, left and right on a slider) and with touch. Edits apply at once as the appearance model's edit layer (`AppearanceModel::edit`), so the whole app repaints through `Environment::theme`. Edits are coalesced: an edit records the value and the theme is rebuilt once, when the next frame reads it, however many edits arrived in between. A colour edit is paint-only: layout keys its retained slots on the theme's geometry (`Theme::lays_out_like`), so nothing is re-measured, and the painter re-records only the nodes whose recorded colours used the edited role (`Theme::paints_like` over the roles each retained record resolved); everything else is spliced. Reset all drops every edit. Export creates the theme folder and its parents through the storage provider (`StorageProvider::create_directories`) and writes `themes/<theme>-edited.toml` through `AppearanceModel::export_edits` into the theme folder (`sdmc:/arisa/themes/` in the console gallery, the assets theme folder next to the executable on the host) or into `InspectorOptions::exports` when set. The file is written in place, which FAT handles without a rename. The line under the buttons shows where the file went (`StorageProvider::location`, for example `sdmc:/arisa/themes/arisa-theme-glass-edited.toml`) or the error, with the storage error and its native code (`themes/...toml:0: the theme could not be written: resource exhausted (error 28)`). An app without `AppearanceOptions` has no tokens to edit and the panel says so.

## Cost

Closed, the inspector does nothing but look for the chord: presents, evaluations, damage and allocations are identical to a build without it (`InspectorTest.ClosedInspectorMatchesABuildWithoutIt`). History is only recorded while it is open.

Open, it records frames the app produced and leaves out frames caused only by its own drawing: `FrameTimings::debug` is set when the debug layer's own evaluations, or input routed to it, were the whole of a frame's work, and `FrameTimings::debug_evaluations` counts its evaluations apart from the app's `evaluations`. An open, idle inspector does no work at all: no presents, evaluations, layout, focus collection, allocations or transaction bookkeeping per loop iteration (`TokensTest.AnOpenIdleInspectorDoesNoWork`, 300 frames with a token slider focused). The overlay only redraws after an app frame when it shows an outline (a selected node, or Pick); the Timeline, Cost and GPU panels read the frame history themselves. With Pause on, history stops and nothing in it redraws.

## The introspection interface

`Application::introspection()` returns a read-only `runtime::Introspection`. The inspector reads the framework only through it:

- `nodes()`, `node(id)`, `costs()`: node records (id, parent, depth, key, readable component name, frame, bounds, padding, flags, `NodeCost::Counts`) for the live tree, without the debug layer.
- `hit(point)` and `points(pixel)`: the deepest node under a point, and pixel-to-point conversion.
- `focus()`: focused node, chain, scopes and next per direction.
- `gpu()`: `GpuProvider::inventory()` plus the frame schedule's hooks, the texture publications and the surface directory, the glyph atlas and the render-target pool.

The inspector changes the tree only through its own actions: presenting or removing its layer, selecting and picking.
