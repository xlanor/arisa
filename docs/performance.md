# Performance

Numbers recorded by `arisa_bench`. The host benchmark runs the same scripted scenarios as the console NRO against the headless providers with a manual 16 ms clock, so host CPU figures are relative regression signals, not console estimates. Console GPU figures come from deko3d timestamp queries (`FrameTimings::interface_gpu` + `effects_gpu`).

## Scenarios

| Scenario | Script | Frames measured |
|---|---|---|
| idle | gallery typography page, no input, after 240 settle frames | 600 |
| fling | 10,000-row `List`, six touch flings down then six up (42 % of the screen height over six frames, then coast) | 720 |
| gallery | every gallery page, R every 45 frames | 1,080 |
| navigation | `NavigationStack` push and pop x20 | 1,200 |
| sheet | `.sheet` open and close x20 | 1,200 |
| theme | gallery scheme switch (Y) x10 | 300 |
| launch | launch screen write-on, Bumbbled outline when `third_party/bumbbled/Bumbbled.otf` exists | 300 |
| launch_path | launch screen write-on, hand-authored path | 300 |
| token_slider_drag | gallery Controls page with the inspector open on Tokens, four 48-frame touch drags of the `colours.accent.light` saturation slider | 240 |

Every scenario except idle and launch is rehearsed once before it is measured, so first-use work (glyph rasterisation, cache and pool growth) is not counted as steady state.

## Budgets

| Budget | Host gate (`ctest -L bench`, in `make check`) | Console |
|---|---|---|
| idle | no repaint without a content change; mean frame wall time < 250 us | 0 presents, ~0 CPU |
| steady-state scroll and animation | 0 heap allocations in repaint frames (frames that neither lay out nor evaluate a body nor receive a new gesture); gallery tolerates 1 % for app tasks resuming | same |
| fling CPU | mean presented-frame CPU < 6 ms (debug, -Og) | < 2 ms handheld |
| fling uploads | mean vertex bytes uploaded per repaint frame < 2 KB | same |
| fling row building | mean heap allocations per frame that evaluates a body < 180 | same |
| split_steps evaluations | body evaluations per selection step <= 10 | same |
| fling GPU | not measured (null GPU) | < 3 ms handheld |
| spinner GPU | not measured | < 0.3 ms |
| launch CPU | mean presented-frame CPU < 1.5 ms (6 ms under ASan), mean uploaded < 2 MB, both launch scenarios | well under 16.6 ms |
| token_slider_drag CPU | mean presented-frame CPU < 3 ms (debug, -Og; 12 ms under ASan); exempt from the repaint-allocation rule because the open inspector records frame history | < 3 ms |

Every bench test is `RUN_SERIAL`: ctest runs each one with nothing else in flight, so a CPU mean is never measured while nine other test processes share the cores. The bench's `cpu` is wall time minus `blocked()`, so preemption and a move to an efficiency core both count against it (token_slider_drag measured 1.6 ms alone, 5.8 ms inside the parallel `make check`, 7.5 to 8.2 ms clamped to the efficiency cores with `taskpolicy -c background`, where thread CPU time inflates as much as wall time does). `bench/glass_scroll_real_time` failed the same way inside the parallel run (sidebar 613 us over 1.25x detail 421 us, which are measured seconds apart under changing load, and one heap allocation in a detail_scroll repaint frame whose present was blocked 54 ms on a starved render thread; the allocation meter counts every thread, and the extra allocation did not land in a repaint frame in six runs under the full suite or with spinning load). Serialising removes the load, but the sidebar-to-detail ratio stays noisy even alone, because a real-time run keeps the UI thread about 2 % busy and the core and clock it gets drift between the two scenarios: four isolated debug runs gave sidebar over detail 0.87, 0.97, 1.08 and 1.24 (medians 0.88 to 1.16), one more 1.36, and ASan runs 0.80 to 1.25 against the 1.25 limit; so the ratio check was removed on 2026-11-14; the deterministic `blurs_per_frame` budget on the manual-clock sidebar_scroll is what pins the re-blur regression. Serialising costs about 27 s of gate time and keeps every budget at its full strictness.

## Host results (2026-11-09, debug -Og, Apple silicon)

Before is HEAD f5e0363 with only the allocation meter and the benchmark added; after is this change.

| Scenario | Allocations before | Allocations after | Repaint allocations before | Repaint allocations after | CPU mean before (us) | CPU mean after (us) |
|---|---|---|---|---|---|---|
| idle | 13,230 | 15 | n/a | n/a | 3 (wall) | 2 (wall) |
| fling | 145,300 | 30,466 | 24,897 in 670 | 0 in 670 | 249 | 243 |
| gallery | 400,785 | 172,315 | 121,526 in 999 | 3 in 999 | 581 | 579 |
| navigation | 62,020 | 4,760 | 51,280 in 1,160 | 0 in 1,160 | 26 | 25 |
| sheet | 46,640 | 2,300 | 41,320 in 1,160 | 0 in 1,160 | 15 | 14 |
| theme | 63,384 | 14,808 | 15,462 in 269 | 0 in 269 | 242 | 236 |
| launch | 33,231 | 11,761 | 13,926 in 217 | 104 in 217 | 1,152 | 1,180 |

10,000-row fling, frames that realise rows (38 of 720): 3,149 allocations and 1,076 us per frame before, 794 allocations and 1,022 us after (cell recycling, reconciler scratch, allocation-free layout sync). The remaining allocations in those frames are element construction in row bodies and Yoga's internal flex-line vectors.

## Row building (2026-11-10, release -O3, Apple silicon)

Frames that build rows used to rebuild far more than the rows that changed. A lazy stack re-evaluated on every window shift rebuilt the element of every row in the window (about 37 in the fling) only for the reconciler to find them equal; a selection step rebuilt every cell of a plain section; and every child list change in Yoga dropped the cached layout of every sibling. Now:

- `composition::Retained` is a run of keyed children the parent asserts are unchanged. The reconciler keeps each matching live child untouched and builds a child only when its key is missing. A lazy stack emits one run for the rows it built last time from the same sources, so a window shift builds only the rows that enter.
- `ListCell` and `Section` follow the `same_as` + `adopt` rule of the built-in controls: equal traits and placement (or an equal row structure) keep the node, and the newer row content is forwarded to the existing placeholder, so the newest row actions still fire. A forwarded placeholder whose single child matches is updated in place instead of being evaluated.
- `ElementContent::ignores_change` lets content decline an environment change. A cell ignores a `RowSelection` change unless its own selected state or the focus binding changed, so a selection step re-evaluates the two cells it changed.
- The reconciler matches positions through borrowed keys (no `ElementKey` copies), keeps its evaluated set in a `flat_set`, and the tree recycles registry entries and rebinds a recycled node's anchor when nothing holds it.
- `YogaSlot::adopt` removes and inserts only the children that changed instead of `YGNodeRemoveAllChildren`, which reset every sibling's cached layout. Yoga's flex line keeps its item list in a per-node scratch vector (a small change to the vendored Yoga 3.2.2: `Node::getFlexLineItems`, and `FlexLine::itemsInFlow` is a span over it).

Before is HEAD ed67a2c, after is this change, min of three interleaved runs. Per building frame is the mean over frames that evaluate a body.

| Scenario | Allocations before | Allocations after | Per building frame before | Per building frame after | Evaluations per step before | after | CPU mean before (us) | CPU mean after (us) |
|---|---|---|---|---|---|---|---|---|
| fling | 30,466 | 5,603 | 794 | 139 | | | 54 | 46 |
| split_steps | 16,188 | 4,549 | 726 | 197 | 54 | 7 | 117 | 94 |
| sidebar_steps | 39,812 | 32,191 | 959 | 773 | 63.5 | 15.5 | 120 | 118 |
| sidebar_fast | 45,865 | 37,124 | 985 | 795 | 63 | 15 | 91 | 89 |
| sidebar_scroll | 450 | 433 | 8 | 7 | | | 60 | 57 |
| materials_page | 105 | 105 | | | | | 174 | 174 |
| glass_page | 92 | 92 | | | | | 84 | 83 |
| gallery | 158,775 | 138,593 | 3,112 | 2,716 | | | 118 | 113 |
| idle | 15 | 14 | | | | | 58 | 57 |

The fling's remaining allocations are the rows that enter the window (about 3 per building frame, about 40 allocations each: the row builder's elements, the cell style's stacks and the cell's state slots). The split step's remaining allocations are the screen body rebuilding its 12 labels and the two cells whose selection changed. The sidebar scenarios rebuild every `TabView` sidebar item on each step; that is the next target.

## Retained draw lists and damage-rect repaint

The painter keeps a retained draw list per node (`rendering::DrawListCache`). After a walk, consecutive quads with the same owning node form a run keyed by (node, ordinal). Each run carries a content hash over everything that reaches its vertices (kind, bounds, uv, shape, colour, effect, detail, clip, resolved transform, mesh vertices and indices, zoom) and an order hash over what only affects drawing (texture, detail texture, the preceding run). Runs live at stable offsets in a resident vertex arena (first-fit free list, compacted when more than half is free); a run whose content hash is unchanged keeps its vertices and is neither re-placed nor uploaded. Indices are rebuilt every frame, so reordering costs no vertex upload. Layer promotion (M7) still produces one layer quad per promoted subtree, marked fresh when its texture re-renders; the compositor's backdrop and field passes and layer passes use a volatile tail of the arena that is rewritten when present.

Providers expose the arena through `GpuFrame::allocate_resident` / `touch`: the headless provider counts the touched ranges as uploads, SDL_GPU keeps one persistent vertex buffer and uploads only touched ranges through a staging buffer (indices are uploaded whole), and deko3d keeps a vertex buffer per frame slot and copies only the ranges touched since that slot last drew. Providers without support fall back to `allocate_shapes` and a full upload.

Damage is computed from the same runs: a changed, added or reordered run damages its old and new bounds (vertex bounds clipped to its scissor, plus one pixel), a departed run damages where it was, a run marked fresh (external textures, newly created pictures, re-rendered layers, a backdrop blur or glass field re-rendered this frame) damages its bounds, and any glass or material run whose backdrop source region intersects the damage is added until the region is stable. A size, clear-colour or zoom change, or a failed frame, repaints everything. The painter hands the union to `GpuFrame::damage`; the provider accumulates it per swapchain image and repaints each image with the union of everything since that image was last drawn, scissoring the clear and every draw command. deko3d does this with the swapchain images it owns and repaints in full whenever a before/after-UI hook is installed (an app drawing video under or over the UI); SDL_GPU repaints in full because swapchain contents are not preserved. The headless provider emulates a 1-3 image swapchain (`NullGpu::swapchain`), which is how partial repaint is tested pixel for pixel against a full repaint.

### Host results (2026-11-09, debug -Og, Apple silicon)

Before is the same build with retention and damage forced off (every run rewritten and uploaded, every frame repainted in full), which is what HEAD 11656f2 uploads and repaints; uploaded bytes are vertex plus index bytes per presented frame, damaged area is pixels per presented frame on the 1280x720 bench surface (921,600 px). CPU before is HEAD 11656f2 itself.

| Scenario | Uploaded before (B/frame) | Uploaded after (B/frame) | Damaged before (px/frame) | Damaged after (px/frame) | CPU mean before (us) | CPU mean after (us) |
|---|---|---|---|---|---|---|
| idle | 189,128 | 12,072 | 921,600 | 0 | 288 (1 frame) | 376 (1 frame) |
| fling | 89,541 | 77,751 | 921,600 | 765,082 | 231 | 257 |
| gallery | 290,119 | 112,083 | 921,600 | 311,605 | 460 | 504 |
| navigation | 2,544 | 2,544 | 921,600 | 921,600 | 23 | 24 |
| sheet | 28,594 | 19,712 | 921,600 | 735,603 | 13 | 17 |
| theme | 189,504 | 147,874 | 921,600 | 868,471 | 201 | 228 |
| launch | 841,205 | 443,653 | 921,600 | 645,122 | 799 | 845 |

A frame with only a spinner animating uploads only the spinner's run and damages only the spinner (`DamageTest.SpinnerFrameDamagesOnlyTheSpinner`); a repaint of an unchanged tree uploads 0 vertices and damages 0 pixels. Scrolling used to re-upload every visible row because transforms were baked into vertices; see "Transform replay" below. The CPU cost of hashing every quad is about 5-10 % on the host debug build in scenarios where most of the screen changes.

## Launch write-on (2026-11-09, debug -Og, Apple silicon)

The write-on used to rewrite every vertex of the stroke each frame: the trim window lived in each vertex (`shape.zw`), so `ShapePainter::patch` rewrote the whole mesh, the painter copied it into its per-frame mesh slot, hashed every byte of it for the retained draw list and for layer promotion, rewrote it into the vertex arena and uploaded it. With the Bumbbled outline the glyph fill is split into ~50,000 vertices for guide projection, so that was 4.4 MB copied, hashed and uploaded per frame (deko3d copies it again into every frame slot). The wallpaper mesh was re-tessellated with perceptual colour mixing at every 15 fps drift step.

Now the trim window is a per-draw parameter (`DrawCommand::trim`, a fragment uniform on deko3d and SDL_GPU, a sampler field in the software raster), so a trim change touches no vertices: the run is marked fresh for damage and the command carries the new window. Meshes carry a revision; the painter keeps the staged copy while the revision is unchanged and hashes the revision instead of the vertex bytes, and promotion reads a cached extent. Drift steps reposition the wallpaper vertices and keep their colours. The stroke is tessellated on the first frame even while hidden, so the write-on start no longer stalls.

| Scenario | CPU mean before (us) | CPU mean after (us) | CPU max before (us) | CPU max after (us) | Uploaded before (B/frame) | Uploaded after (B/frame) |
|---|---|---|---|---|---|---|
| launch (Bumbbled) | 2,523 | 575 | 18,941 | 21,906 (first frame) | 4,388,953 | 1,012,290 |
| launch_path | 1,194 | 335 | 6,477 | 6,779 (first frame) | 443,653 | 269,744 |

Write-on frames 16-150 only: Bumbbled 2,360 -> 531 us mean, hand path 969 -> 285 us. Most of what remains uploaded is the index buffer, which is rebuilt every frame by design.

## Transform replay (2026-11-09, debug -Og, Apple silicon)

A quad's translation is now a per-command parameter, like trim: `DrawCommand::translation` (with `DrawCommand::opacity`) is a vertex uniform on deko3d (binding 1) and SDL_GPU (vertex slot 1), and the software raster and the NullGpu shape record apply it. The painter splits each quad's resolved transform (`Painter::replay`): the linear part stays baked into the vertices and the translation goes to the command, except for backdrop-sampling quads (their effect region is in screen space), layer quads and zoomed frames, which stay fully baked. Glyphs keep their whole-pixel snap: the shader rounds the translation for glyph vertices, and a glyph under a scale or rotation bakes the sub-pixel remainder. Content hashes use the baked part only, runs split where the translation changes, and commands split on a different translation, so rows under one scroll offset still batch into one command. Each retained run stores its untranslated vertex box, its clip and its offset; a run whose offset changed without a content change is not rewritten, and damage covers its old bounds and its new bounds (the local box moved by the new offset, clipped). The compositor's backdrop hash takes the per-span translation into account.

Before is HEAD 324eaf2 with only the vertex-bytes bench column added (uploaded columns) and HEAD itself (CPU, interleaved runs, mean of three).

| Scenario | Vertex bytes before (B/frame) | Vertex bytes after (B/frame) | Per repaint before | Per repaint after | Uploaded total before | Uploaded total after | CPU mean before (us) | CPU mean after (us) |
|---|---|---|---|---|---|---|---|---|
| fling | 72,035 | 1,668 | 72,490 | 1,059 | 77,751 | 7,383 | 268 | 258 |
| gallery | 80,605 | 70,126 | 73,334 | 62,639 | 113,835 | 103,357 | 520 | 527 |

The remaining fling repaint uploads are the scroll indicator and rows entering the window; what remains of the total is the index buffer, which is rebuilt every frame. Gallery repaint frames are dominated by animations that change content (spinners, progress, trims of non-GPU paths). Damage is unchanged within 0.1 % (fling 765,082 -> 765,935 px). `DamageTest.ScrollingReplaysRowsWithoutUploadingThem` and the swapchain twin tests (partial repaint equals full repaint pixel for pixel while scrolling) cover it; the bench gates fling at 2 KB of vertices per repaint frame. Scale and rotation are still baked.

## Token slider drag (2026-11-14, debug -Og, Apple silicon)

On a Switch 2 a saturation drag in the inspector's Tokens panel ran at 13.3 ms average and 27 ms max UI CPU per frame (86 % of the UI thread). Every slider step re-applied the whole theme and everything downstream treated it as a new theme:

- Layout keyed every retained Yoga slot on the theme's address, so a new theme object re-synced all ~1,800 nodes. Slots now key on a geometry key (`Theme::lays_out_like`: typography, spacing, metrics, radii, glass, preferences, Dynamic Type size), so a colour-only theme keeps every slot.
- The painter's retained records were keyed on the canonical theme pointer, so every node was re-recorded. Each record now carries the set of colour roles its subtree resolved through `Scope::colour`, and a record recorded under another theme is still spliced when `Theme::paints_like` says every non-palette field and every role it used are unchanged.
- `ElementType::of<T>()` parsed `__PRETTY_FUNCTION__` at run time on every call (memchr was the top self-time symbol); it now returns a constant-initialised variable template.
- The Tokens panel rebuilt every row element on each edit; rows are now `TokenRow` components compared by value, so only the edited row and its editor are evaluated.
- `AppearanceModel::edit`, `undo` and `reset_edits` only record the change and mark the theme stale; the theme is rebuilt once when the next frame reads it.

| | Before (HEAD 71cb1c6 + scenario) | After |
|---|---|---|
| CPU mean (us) | 9,431 / 8,901 | 1,379 / 1,492 |
| CPU p95 (us) | 19,155 / 18,386 | 2,041 / 2,459 |
| allocations | 271,895 | 122,807 |
| nodes re-recorded per drag frame (of 1,763) | ~1,763 | ~130 |
| nodes laid out per drag frame | ~1,763 | ~9 |

Two runs each, interleaved. The console ran this drag at 13.3 ms against 9.2 ms on this host build, so the after figure corresponds to roughly 2 ms on a Switch 2. What remains per drag frame: the app's own theme readers (Scaffold, NavigationSplitView, TabView and GalleryScreen read `Environment::theme` for metrics, glass and the edge-fade colour, so a colour edit still re-evaluates and re-reconciles the app chrome), a focus collection because the environment change bumps the tree revision, the Yoga pass along the dirty path (the edited row's value text and the slider), and a full re-record once every 32 distinct themes when the painter drops its canonical theme cache (max frame 2.5–6.6 ms).

## Render thread

When the GPU provider can present from another thread (`GpuProvider::presents_off_thread`: NullGpu, deko3d and Vulkan; SDL_GPU must acquire its swapchain on the window thread and stays inline), `Application` starts a `runtime::RenderThread` on a `ThreadRole::render` thread (core 1 at UI priority on the console; `ApplicationOptions::render_thread = false` keeps everything inline). It replaces the animation worker. The handoff point is `present()`:

- The UI thread (core 0) evaluates, reconciles, lays out, routes input, walks the painter and writes the vertices into the frame slot's mapped memory (and the resident mirror), then seals the `GpuFrame` into an immutable `provider::FramePacket`: the plan (predicted present, replay, exclusive), the serial, the frame number and the slot. The slot's recording, which nothing touches until the packet is submitted, holds the draw lists, damage, the surface underlays and due texture surfaces resolved at seal time, the external-texture fences drained at seal time, the deferred texture uploads (deko3d, null GPU) or the upload commands already in the slot's command buffer (Vulkan), and which resident ranges to copy.
- The render thread submits each packet (`FramePacket::submit`): copies the resident vertices into the slot's GPU memory, waits for the next slot's fence, releases the next slot to the UI thread, acquires the display buffer, records the command buffer, runs frame hooks and surface functions, submits and presents. It also builds and presents the replay frames that the animation worker used to (same rules: only after the UI thread missed a whole frame, at the hint rate, until the tracks settle).
- Back-pressure, never drops: `provider::FrameSlots` lets the UI thread build frame n+1 only once the render thread has started frame n (after its vertex copy and the next slot's fence), and the runtime queue holds one packet. The UI thread blocks (counted as `FrameTimings::stall`) when the render thread is a full frame behind. A packet that is destroyed unsubmitted (only at teardown) is abandoned and still releases its slot; `Application::drain()` waits for every queued packet.
- Under a stepped clock (tests, the host bench) the UI thread waits for its packet to be submitted before `frame()` returns and replays run only on `step_animations()`, so frames, pixels and every `FrameTimings` counter are identical with the render thread on and off (`RenderThreadTest.LockstepFramesMatchTheInlinePath`).
- Since round R2 there is no frame gate. Replays never claim a UI frame slot: each provider has a replay slot of its own (`GpuProvider::begin_replay`, the null GPU's replay frame, Vulkan's and deko3d's extra `FrameSlot`) and draws the replay's own indices and patched commands over the resident vertices of the last UI frame it submitted (`GpuFrame::replay`, `ReplayDrawData`). The scene a replay needs is an immutable snapshot from a pool of four in `RenderThread`: the UI thread fills a free one while painting and hands it over with its packet, and the render thread replays only the scene of the last packet it submitted, so vertices and scene always match. The pool's bookkeeping and the one-packet queue share a short leaf mutex that nobody holds across a build; the render thread never waits for the UI thread's build. A replay reserves its serial with a compare-and-swap that succeeds only when no UI frame has sealed since the last submission, so serials stay in submission order without a lock.
- Lifetimes cross the boundary by serial: draws are marked with the packet's serial when it is sealed; textures destroyed while a frame (UI or replay) is open keep their registry entry until every frame opened before the release is closed (`TextureRegistry::open` returns a ticket, `close(ticket)`) and their GPU memory until its serial completes; surface removal and render-target retirement go through the same path.
- deko3d: texture uploads are copied into staging memory on the UI thread and recorded at the start of the packet's command list, with a fragment barrier before the copies. Before this round an upload was submitted on its own with no barrier before the copy, so a reused glyph-atlas region could be overwritten while an earlier frame still sampled it; that hazard is fixed. Vulkan already had a sampling-to-transfer barrier before every upload and needed no fix. Since R2 the Vulkan renderer lock is never held while recording: the UI thread owns its building slot between claim and seal (allocations, touches and the resident mirror take no lock), textures used by a frame are resolved into a per-frame table when it is sealed, the render thread records with its own barrier recorder, and the lock is taken only for short bookkeeping (graveyard, descriptor pools, fence polls, the `vkQueueSubmit` itself). `VulkanGpu::Statistics::allocation_waits` counts UI-side lock contention; `VulkanGpuTest.UiAllocationsNeverWaitForALongRecording` holds an 80 ms hook on the render thread and checks the UI thread's begin, resident and shape allocations and a texture create and upload take under 40 ms and add no waits.

`FrameTimings` now splits the work: paint = `walk` (tree walk and draw-list recording) + `upload` (atlas flush) + `emit` (vertex writes); `present` is UI-thread time in `begin_frame`, sealing and the handoff, of which `blocked()` is waiting (`stall` for the slot and the queue, plus the provider wait when inline); `record` and `wait` are the provider's CPU work and its acquire/vsync/fence waits for the packet (the frame's own packet in lockstep, the latest submitted one otherwise), measured on deko3d, Vulkan, SDL_GPU and the null GPU. `FrameBreakdown::describe`, the inspector timeline (new Walk, Present and Blocked colours on the CPU chart, a Record/Wait render-thread chart) and the bench CSV (`walk_us,upload_us,emit_us,present_us,present_cpu_us,blocked_us,record_us,wait_us,threaded`) show the split, and the bench's `cpu` is now wall minus `blocked()`.

### Host phase split (2026-11-14, debug -Og, Apple silicon, `arisa_bench --real-time`, null GPU without raster)

Means over presented frames, in microseconds. "Before" is the build before this round (UI-thread time is wall, since nothing waited). Render thread on, then inline (`--inline`) for the same build:

| Scenario | before wall | threaded wall | walk | emit | present (cpu / blocked) | render record / wait | inline wall | inline present (cpu / blocked) | inline record |
|---|---|---|---|---|---|---|---|---|---|
| fling | 740 | 724 | 393 | 107 | 13 (4 / 10) | 17 / 0 | 1,008 | 27 (22 / 5) | 19 |
| gallery | 1,071 | 1,190 | 722 | 131 | 16 (4 / 12) | 30 / 0 | 1,011 | 31 (28 / 3) | 26 |
| navigation | 346 | 340 | 203 | 28 | 15 (3 / 12) | 6 / 0 | 279 | 10 (7 / 3) | 5 |
| sheet | 224 | 225 | 88 | 38 | 12 (3 / 9) | 9 / 0 | 278 | 14 (10 / 3) | 8 |
| theme | 1,190 | 1,007 | 729 | 146 | 9 (2 / 6) | 13 / 0 | 1,463 | 24 (21 / 3) | 18 |

On the host the null GPU's submit is 5-30 us and never waits, so the render thread can only take that much off the UI thread; the run-to-run noise of the real-time host bench (+-30 %) is larger than the effect, and the walk dominates every scenario. The split is in place for the console, where `record` (command-buffer recording) and `wait` (acquire, vsync and fence waits) are the large parts of today's `present`. Under the manual clock (`arisa_bench` without `--real-time`, the budget gate) allocations are unchanged: fling, navigation, sheet and theme 0 in repaint frames, gallery 4 of 999 (3 before, 1 % tolerated), and the means are within noise (fling 99 -> 114 us, gallery 442 -> 274 us, navigation 53 -> 43 us, sheet 15 -> 19 us, theme 492 -> 271 us).

Tests (`RenderThreadTest`, `RenderThreadRealTimeTest`, `NullGpuTest`, in `make tsan`): the animation-worker cases (animations advance while the UI thread is blocked or a body evaluation blocks it, replays start after one missed frame, honour a 30 fps hint and stop when settled); lockstep frames, pixels and counters identical inline and threaded; UI frames and replays all submitted from one thread, never overlapping; a 4 ms hook (slow GPU) makes the UI thread stall and every frame is presented; an external texture retired while packets use it releases exactly once after they complete; texture surfaces added and removed while frames are in flight are never called after removal; packets, replays and an importer thread publishing and importing together; a texture retired while a frame is open outlives that frame; abandoned packets release their slot.

### Round R2 (2026-11-14): lock-free replay, pacing, narrowed Vulkan lock, queue lease, packet capture

- Replays without a gate (see the bullet above). `RenderThreadRealTimeTest.ReplaysContinueWhileTheUiThreadHoldsItsBuild` holds the UI thread inside `begin_frame` for 150 ms during an animation and sees at least 4 replays presented meanwhile (the old gate presented none); `HeldBuildsReplaysAndPublicationsRunTogether` mixes held builds, replays and an importer thread under TSan.
- Earlier input: `Application::frame` samples input first (before reconcile), routes it after layout and performs the actions before painting; when an action fired it reconciles and lays out once more, so the effect is painted in the frame that sampled the input instead of the next one (`FramePacingTest.EarlierInputShowsAPressInTheFrameThatSampledIt`). The bench marks the release frame of a scripted press (`Sample::released`) so the release's own work is not counted as a steady repaint.
- Wait at the start: `runtime::FramePacer` keeps the latest `FrameReceipt::displayed` as the vblank phase and the worst of the last 16 build costs (UI busy time plus the render thread's record time); `Application::next_build()` is the next reachable vblank minus that cost and 2 ms, `waiting()`/`wait()` sleep until it, the frame's `FrameInfo::predicted_present` is that vblank, and the software rate skip is off while it is active. It is active only with continuous frames and a provider that reports display times.
- Swap interval: frames whose rate hint is 30 fps or less carry `FramePlan::swap_interval = 2` (null GPU and Vulkan record it, deko3d and ounce apply it).

UI-thread CPU, `arisa_bench --real-time` (debug -Og, null GPU without raster, render thread on), mean / p95 in microseconds, before and after this round on the same machine: fling 606 / 1,064 -> 773 / 1,373 (a second run: 674 before, 584 after), gallery 837 / 2,616 -> 1,086 / 2,776, navigation 324 / 503 -> 326 / 584, sheet 159 / 271 -> 189 / 339, theme 1,262 / 2,001 -> 931 / 1,929. These are within the ±30 % run-to-run noise of the real-time host bench; the round moves no UI work except the second reconcile on frames where an action fired. The manual-clock budgets (`--budgets`) are unchanged apart from the CPU means that already exceeded their budgets under host load before the round.

Vulkan: 0 `allocation_waits` and under 40 ms for a begin, resident and shape allocation, texture create and upload made while the render thread sits 80 ms in a hook; a replay over the last frame's vertices draws the shape at its new translation on MoltenVK; `present_interval` follows the plan.

Gates at the end of R2 (host, 2026-11-14): `make fast` green (205 s); `make check` green after two pre-existing fixes (clang-format 23 drift in 23 untouched files, and six console-only example sources excluded from the host clang-tidy list because they have no host compile command), except `bench/token_slider_drag` failing its 3 ms CPU budget at 5.8 ms under the full parallel run and passing alone three times (0.9 s each, 324 s total); `make asan` green (218 ctest entries, 447 s); `make tsan` green (25 ctest entries including the Vulkan suite, plus 8 gallery tests, 133 s); `make leaks` green (37 binaries clean, 50 s); `make package-check TARGET=macos` green (112 s); `make nx` and `make ounce` build (108 s and 153 s), console paths compile-only.

## Console results

| Release | idle presents | fling CPU mean / p95 | fling GPU mean | spinner GPU | repaint allocations |
|---|---|---|---|---|---|
| pending first run of `arisa_bench.nro` | | | | | |

Run `build/nx/arisa_bench.nro` from hbmenu (copy it to `sdmc:/switch/arisa/arisa_bench.nro`). It plays every scenario in order with injected input, logs one `bench <scenario>: ...` summary line per scenario to nxlink and `sdmc:/switch/arisa/logs`, writes every frame to `sdmc:/arisa/bench/<YYYYMMDD-HHMMSS>.csv` (`scenario,frame,kind,presented,laid_out,evaluations,stimulus,cpu_us,wall_us,gpu_us,allocations,uploaded_bytes,damaged_pixels,uploaded_vertex_bytes`) and shows the summaries on screen; press + to exit.

## Streaming budget

The overlay UI must add under 1 ms of GPU time over a 60 fps external video texture and must never copy or block the decode-to-present path. An app hands its own texture to the UI with `components::Image::External(TextureId)`; the painter samples that texture in place as an image quad and never uploads, copies or re-renders it.

Host part (`StreamingBudgetTest.OverlayNeverCopiesOrBlocksTheVideoTexture` in the gallery suite, part of `make check`): a synthetic 1280x720 decoder updates its texture every frame at 60 fps under a HUD (text, two buttons, capsule background). Over 600 measured frames after 120 warm-up frames it asserts that the overlay frame itself performs 0 texture uploads and 0 uploaded bytes, 0 offscreen passes and 0 render targets, presents every frame with 0 discarded, and that every presented frame's draw commands sample the decoder's texture.

| Host result (2026-11-09, debug -Og, null GPU) | Value |
|---|---|
| overlay frame wall time | mean 11 us, p95 13 us |
| allocations per frame | 5 (the video component's element is rebuilt when the decoder publishes a frame) |
| overlay uploads, passes, render targets, discarded frames | 0 |

Console part (not yet run): build the same scene into a console scenario with the deko3d provider, feed it from the NVTEGRA decoder, and read `FrameTimings::interface_gpu` + `effects_gpu` (deko3d timestamps) per frame; the budget is a mean under 1 ms and no frame where the UI waits on the decoder's fence. Record mean, p95 and max in the table below.

| Release | overlay GPU mean / p95 / max | overlay CPU mean | blocked frames |
|---|---|---|---|
| pending first console run | | | |

## Soak

`make soak` runs `arisa_soak` on the host for 300 simulated seconds (16 ms manual clock steps, about 7 s of wall time) and fails when the live allocation count sampled at the end of every cycle (gallery paging with a drag per page and a theme switch every sixth page, a 10,000-row fling, sheet open/close, navigation push/pop, settle) rises after every cycle by 64 or more in total. It prints one row per 30 s and writes `build/host-debug/soak.csv`.

On the console, copy `build/nx/arisa_soak.nro` to `sdmc:/switch/arisa/arisa_soak.nro` and launch it from hbmenu (or `nxlink -a <console ip> arisa_soak.nro --minutes 240`). It runs for 120 minutes unless `--minutes N` is passed, press + to stop early. Every minute it logs `soak <elapsed> s: ...` to nxlink and `sdmc:/switch/arisa/logs`, and appends a row to `sdmc:/arisa/soak/<YYYYMMDD-HHMMSS>.csv` (`elapsed_s,cycles,frames,presented,live_allocations,allocations,heap_bytes,heap_high_water,cpu_p50_us,cpu_p95_us,cpu_p99_us,cpu_max_us,suspensions,resumptions`). Sleep/wake and applet focus changes reach it through the libnx applet hook and are logged as `soak applet <event>`; put the console to sleep and open the HOME menu during the run to exercise them.
