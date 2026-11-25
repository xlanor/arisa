# GPU content under and over the UI

arisa can share its frame with an app that draws its own GPU content: a video player, an emulator, a game with a UI shell, a 3D viewer. The app records its passes into arisa's command buffer through frame hooks, draws into views through surfaces, or hands arisa images it rendered elsewhere as external textures, and the result is presented in the same frame as the UI. `examples/sample` is the place to start: one short app that uses each of these the way an app should, on deko3d (Switch 1), Vulkan (Switch 2) and SDL_GPU (desktop). `examples/gpu_stage` is the feature test bench behind it, with every mode, format and option on its own page.

## Start here: the sample

The sample draws an animated plasma under its whole UI with a frame hook, puts a glass sidebar over it, and lists two cards in a scroll view: a `SurfaceView` showing a second animation, and an image the app repaints ten times a second on its own and hands to arisa as an external texture. Full screen hides the UI and leaves the plasma; B brings the UI back. Pause stops every animation and the app presents nothing at all until something changes. `build/host-debug/sample` runs it on the desktop (`--offscreen 1280x720 --frames 30 --capture sample.png` without a window), and `arisa_sample.nro` on each console.

Only the drawing code differs per backend. The app code is shared:

| File | What it is |
|---|---|
| `animation.hpp/.cpp` | `Animation`, the clock the GPU code reads. Thread-safe, because hooks and surfaces run on the render thread while the UI thread reads it. |
| `sample_renderer.hpp` | `SampleRenderer`, the interface each backend implements: the hook, the surface, the picture shown now, and `repaint_picture()`. |
| `sample_model.hpp/.cpp` | `SampleModel`, the UI state: playing, full screen and the picture's `TextureId`, as `Observable`s. |
| `sample_screen.hpp/.cpp` | `SampleScreen`, the UI. |
| `sdl_sample_renderer`, `deko3d_sample_renderer`, `vulkan_sample_renderer` | One class per backend: pipelines, the hook, the surface and the picture ring. |
| `main.cpp`, `switch_main.cpp` | The desktop and console entry points. |

### 1. The clock

`Animation::advance(pattern, info)` moves the animation by the step between this call's `FrameInfo::predicted_present` and the latest one it has seen, capped at 100 ms, and only while playing. Hooks and surfaces call it with the `FrameInfo` they are given, never with wall time read inside the call, so a frame replayed late still shows the right moment and a paused animation stays exactly where it stopped. The picture, which is drawn outside any frame, reads `latest()`. The three draws use the same time and three patterns from the gpu stage's `plasma.frag`: plasma under the UI, ripples in the surface, a tunnel in the picture.

### 2. The renderer

Each backend's `create(gpu, animation)` does four things, in this order:

1. Loads the plasma shaders (`PlasmaShaders`, compiled for the target by `arisa_add_shaders`, see Shaders below) into a pipeline.
2. Registers the hook: `gpu.add_frame_hook(FrameStage::before_ui, function, {.drive = FrameDrive::continuous})`. The function draws the whole target on every call and binds everything it uses. The returned `FrameHook` is the registration, kept as a member, so destroying the renderer unregisters it.
3. Registers the surface: `gpu.add_surface({.mode = SurfaceMode::texture, .drive = FrameDrive::continuous}, function)`. arisa owns a texture the size of the view; the function draws into the target it is given, inside the viewport and scissor of the `SurfaceFrame`.
4. Makes a ring of three images, imports each with `import_texture`, and paints the first one so the UI never shows an empty image.

`repaint_picture()` takes the next image in the ring. If arisa may still sample it (`in_use()`), it does nothing and the shown image stays; otherwise it draws into it on the app's own queue or command buffer, publishes it with its fence (`publish(dk::Fence)` on deko3d, `publish(ReadySemaphore)` with the next value of a timeline semaphore on Vulkan, `publish()` after submitting on SDL_GPU), and returns its id. A single image would not work: while frames keep coming, the image on screen is sampled by every frame in flight and never becomes idle, so a ring of at least two is needed even for a producer that always shows one picture.

### 3. The model

`SampleModel::play(false)` stops the clock, switches the hook and the surface to `FrameDrive::on_demand` and requests one last frame from each. From then on nothing asks for frames: the app idles and presents nothing (the sample test counts 0 presents over 300 ms). `play(true)` switches both back to continuous. `set_full_screen` and `repaint_picture` set the observables the UI reads.

### 4. The screen

- The root is an `HStack` with no background, so it is transparent and the hook's plasma shows wherever the UI draws nothing (see Transparent UI). The sidebar is glass. Glass blurs only what arisa draws, and the hook draws outside arisa's backdrop, so the sidebar shows the blurred clear colour, not the plasma: that is the documented behaviour (see Known limitation: glass over hook content), and the sample test checks the sidebar's pixels stay exactly the same while the plasma moves. To have glass refract full-screen content, put the content in the base layer (`container_background(ContainerBackgroundPlacement::window, ...)`, see The base layer) instead of a hook; the sample keeps the hook because the hook is what it demonstrates.
- The first card is `SurfaceView(renderer.card())` with a fixed frame inside a `ScrollView`. Its "Surface" caption is glass, and glass refracts a texture-mode surface (decision D4), so the caption shows the ripples blurred.
- The second card is a small component, `Picture`, that reads `model.picture()` and shows `Image::External(id)`. Publishing an image repaints it without evaluating anything; changing the id evaluates only `Picture`.
- `.task(ElementKey{playing}, ...)` runs the picture loop: while playing, `repaint_picture()` and `async::sleep(100 ms)`. The key restarts it when playing changes, and the paused task returns at once, so a paused app publishes nothing.
- `.exclusive_frame(full_screen)` hides the main tree, keeping no layers, so the frame is the clear and the plasma. `.on_exit_command` beside it is how B releases it; release restores focus and repaints the UI whole once.

### 5. The entry point

`main.cpp` creates, in order: the factory, `Animation`, the backend's renderer, `SampleModel`, the `Application`, then the inspector with `Inspector::attach(*application)` behind `#ifdef ARISA_INSPECTOR` (see docs/inspector.md). Destruction runs the other way, so the inspector and the application go before the model, the renderer (which unregisters the hook and the surface and retires the ring) and the clock. `switch_main.cpp` is the same with `HorizonProviderFactory`, the console renderer chosen by `__SWITCH2__`, and Plus to quit.

### Per backend

| | SDL_GPU (desktop) | deko3d (nx) | Vulkan (ounce) |
|---|---|---|---|
| Sample class | `SdlSampleRenderer` | `Deko3dSampleRenderer` | `VulkanSampleRenderer` |
| Hook function | `(const Sdl3Gpu::FrameTarget&, const FrameInfo&)` | `(dk::CmdBuf&, const Deko3dGpu::FrameTarget&, const FrameInfo&)` | `(VkCommandBuffer, const VulkanGpu::FrameTarget&, const FrameInfo&)` |
| Hook target | swapchain texture, begin a render pass on it | swapchain `dk::ImageView`, `bindRenderTargets` | swapchain image in `COLOR_ATTACHMENT_OPTIMAL`, outside a render pass |
| Surface target | arisa's texture in the swapchain format | arisa's `RGBA8_Unorm` image | arisa's `R8G8B8A8_UNORM` image, outside a render pass |
| Framebuffers | none | none | cache per image index and generation (`VulkanPlasmaProgram::target`); one program per target kind, since a surface and a swapchain image can share index 0 |
| Uniforms | `SDL_PushGPUFragmentUniformData` | `pushConstants` into one small buffer | `vkCmdUpdateBuffer` between barriers, one buffer slot per draw site |
| Imported image | `SDL_GPUTexture*` made on `Sdl3Gpu::device()`, sampler usage | `dk::Image` in arisa's memory, released by the import's callback | `VulkanImage` on `VulkanGpu::device()`, sampled usage, left in `SHADER_READ_ONLY_OPTIMAL` |
| Picture drawn on | its own command buffer, `cycle = true`, submitted | its own `dk::Queue` and command buffer | its own command pool and buffer (`VulkanRecorder`), submitted through `VulkanDevice::submit` |
| Publish | `publish()` after submit | `publish(fence)`, signalled after the draw | `publish(ReadySemaphore{semaphore, value})` from a timeline semaphore (`VulkanTimeline`) |
| Hooks and surfaces run on | the UI thread | the render thread | the render thread |
| Completion learnt | frame fences polled at the next frame | when a frame slot is reused | frame fences at the next frame or `completed()` |
| Hook GPU time | not measured | `FrameTimings::hooks_gpu` | `FrameTimings::hooks_gpu`, one frame late |
| Shaders | SPIR-V, glslang | `.dksh`, uam | SPIR-V, glslang |

The Vulkan class reuses three small classes from the gpu stage rather than repeating them: `VulkanPlasmaProgram` (the render pass and pipeline per format, the uniform buffer and descriptor sets, and the framebuffer cache), `VulkanRecorder` (a command pool, buffer and fence, plus the layout barriers) and `VulkanTimeline`. Their arisa-facing parts are the ones in the table.

### Shaders

`arisa_add_shaders` (in `cmake/arisa_shaders.cmake`) compiles GLSL for whichever console or desktop is being built and embeds the result in a static library, so an app writes its shader list once:

```cmake
arisa_add_shaders(my_shaders
    HEADER my_shaders.hpp
    NAMESPACE my::app
    CLASS MyShaders
    INCLUDE src
    SHADERS
        vertex=src/scene.vert
        fragment=src/scene.frag)
target_link_libraries(my_app PRIVATE my_shaders)
```

Each stage is named by its file extension. On nx it runs `uam` and embeds `.dksh`; everywhere else (desktop SDL_GPU and ounce Vulkan) it runs `glslangValidator -V --target-env vulkan1.0` and embeds SPIR-V. The app writes the header, a class with one static `name() -> std::span<const std::uint8_t>` function per stage; the bytes are 4-byte aligned for the Vulkan loader. A shader that runs on SDL_GPU and Vulkan puts its uniforms in `set = 3` under `#ifdef VULKAN`, as `plasma.frag` does. The sample and the gpu stage share `arisa_plasma_shaders`, built this way from `examples/gpu_stage/plasma.vert` and `plasma.frag`; the compiled SPIR-V and `.dksh` are byte for byte what the gpu stage compiled before.

### What the tests check

`tests/examples/sample_tests.cpp` (label `examples`) runs the desktop sample's code offscreen through SDL_GPU: over 300 ms the plasma between the cards changes, the sidebar's interior does not change by a single level, the glass caption over the surface changes with it, and the picture changes as it is republished; after Pause no frame is presented for 300 ms and the plasma does not move, and Play resumes it; Full screen presents frames with no UI quads and the plasma where the sidebar was, and release restores the sidebar pixel for pixel and the focus. `tests/examples/shader_embedding_tests.cpp` checks that `arisa_add_shaders` embeds each compiled binary byte for byte. `VulkanContentTest.TheSampleRunsOnVulkanWithItsHookSurfaceAndPicture` runs the same app through the Vulkan provider on the host (MoltenVK): the hook runs, the picture's publishes are waited on with the timeline semaphore, and the plasma moves. The console builds of the sample are compile-only: neither NRO has run on a console.

## Threads and lifetimes, in one place

- Hooks and surface functions run on the thread that submits frames: the render thread on deko3d, Vulkan and the null GPU (for the frames the UI thread painted and for replays alike), and the UI thread on SDL_GPU or when `ApplicationOptions::render_thread` is off. They never run concurrently with themselves or with each other for one Root. They only record: no submit, flush, wait or fence on the CPU, and no access to the tree or to `Observable`s. Share values with them through atomics or a locked type such as the sample's `Animation`.
- Since the render thread round R2 a Vulkan hook or surface function no longer runs under arisa's renderer lock: the render thread records the frame from a table of texture handles resolved when the frame was sealed, so `VulkanGpu::completed` is safe to call from inside one. It still must not submit or wait, and must not destroy its own registration.
- An app that submits its own work to the queue arisa presents on (`Deko3dGpu::queue()`, or arisa's `VkQueue` through `VulkanDevice::submit`) takes a `RenderQueue::Lease` around it (see The render queue), so its submissions never interleave with a frame's submit and present.
- `FrameHook` and `GpuSurface` are the registrations. Destroying one unregisters it and waits only for a call already in flight, so it is safe from any thread except from inside its own function. `set_drive` and `request_frame` are safe from any thread.
- `import_texture` and `publish` are safe from any thread and never block. An image may be rewritten only while it is neither shown nor `in_use()`; `in_use()` stays true until the last frame that sampled it has completed on the GPU, which needs frames to keep presenting, so keep a ring.
- Destroying an `ExternalTexture` releases nothing still in use: arisa keeps the descriptor and runs the `released` callback once the last frame that sampled it has completed, on whichever thread learns of it. Free the image's memory there, or after `in_use()` turns false.
- A texture-mode surface's texture is replaced on resize and retired by `GpuSerial`; on Vulkan its `FrameTarget::generation` changes, so keep a framebuffer for an old generation a few frames before destroying it.
- Declare the inspector after the `Application`, the `Application` after everything its components reference, and the GPU objects (renderer, rings) after the provider factory, so destruction runs in the safe order. Wait for the device (Vulkan) or the app's own fences (deko3d) before freeing images the app drew into.

## Registering a hook

Every GPU provider that can present has `add_frame_hook(stage, function, options)`:

- `Deko3dGpu::add_frame_hook`, whose function receives `dk::CmdBuf&`, the `FrameTarget` (image, view, format, width, height) and a `FrameInfo`.
- `Sdl3Gpu::add_frame_hook`, whose function receives the `FrameTarget` (command buffer, swapchain texture, format, width, height) and a `FrameInfo`.
- `VulkanGpu::add_frame_hook`, whose function receives arisa's `VkCommandBuffer`, the `FrameTarget` (image, image view, format, extent, layout, swapchain image index and target generation) and a `FrameInfo`. The layout contract is under Vulkan hooks below.
- `NullGpu::add_frame_hook`, whose function receives the software canvas when the null GPU is rasterising (an empty optional otherwise) and a `FrameInfo`. It exists so that tests can exercise hooks headless.

The stage is `FrameStage::before_ui` or `FrameStage::after_ui`. Each frame runs clear, then every `before_ui` hook, then the UI, then every `after_ui` hook, all in one command buffer and one present. Several hooks may share a stage; they run in the order they were added.

The call returns a `FrameHook`. It is the registration: destroying it unregisters the hook. Destruction waits only until no call of that hook is in flight, so it is safe from any thread, except from inside the hook itself, which would wait on its own call.

## Vulkan hooks

A Vulkan hook is called outside any render pass, with the target image in `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` (`VulkanGpu::hook_layout`, also given as `FrameTarget::layout`). Before the call arisa ends its own render pass and records a barrier that makes its writes to the image (the clear, or the UI for an `after_ui` hook) available to every stage and every access; after the call it records a barrier from every stage's writes back to colour-attachment access, and begins a new render pass for whatever follows. The hook therefore needs no synchronisation of its own against arisa, and it may use a classic render pass (with `initialLayout` and `finalLayout` both `COLOR_ATTACHMENT_OPTIMAL`, load op `LOAD` to keep what is underneath or `DONT_CARE` when it covers the whole target), dynamic rendering if the app enabled it on its own, or commands that are only legal outside a render pass, such as `vkCmdUpdateBuffer` for its uniforms. It must leave the image in `COLOR_ATTACHMENT_OPTIMAL` and must not end inside a render pass. Calling outside a render pass was chosen over calling inside arisa's pass because it needs no render pass compatibility between the app's pipelines and arisa's, works the same on NVK and MoltenVK without any device feature, and lets the hook update buffers.

The swapchain images only promise colour-attachment usage, so a hook draws into the target; it cannot copy or clear it with transfer commands. `FrameTarget::index` and `generation` let a hook cache a framebuffer per swapchain image: the generation changes whenever arisa recreates the swapchain, and arisa waits for the device to go idle before doing so, so a framebuffer cached for an older generation can be destroyed as soon as the new one is seen. The gpu stage's `VulkanPlasmaProgram::target` does exactly that.

After a hook, arisa rebinds everything it uses (pipeline, descriptor sets, vertex and index buffers, push constants, viewport and scissor), so a hook may leave any of them bound.

## FrameInfo

Each call gets an immutable `FrameInfo` built for that frame:

- `serial`: a `GpuSerial` that rises by one for every presented frame, replays included. A replay reserves its serial only when no UI frame has taken one since the last submission (`FrameSchedule::next_serial_after`), so serials always reach the GPU in order.
- `predicted_present`: when the frame is expected on screen, on the same clock as the app's `ClockProvider`. Advance animations by this value, not by wall time read inside the hook, so that a frame replayed late still shows the right moment.
- `extent`: the target size in pixels.
- `replay`: true when the frame is a replay the render thread built from the last UI scene, false for a frame the UI thread painted.
- `damage`: the region the UI changed this frame, or empty when the UI repainted everything. It is informational: a hooked frame repaints the whole target.

On deko3d, `Deko3dGpu::completed(serial)` says whether the GPU has finished the frame with that serial. It turns true no later than when that frame's slot is reused, so it can lag by the swapchain depth. Use it to know when a buffer the hook wrote for an old frame may be reused. `VulkanGpu::completed(serial)` answers the same question; it polls the frame fences when it is called, so it does not lag.

## Driving frames

`FrameHookOptions::drive` says who asks for frames:

- `FrameDrive::continuous`: the hook keeps frames coming even while the UI is idle, at `frame_rate` when one is given and at the display rate otherwise. When the UI is also animating, the faster of the two wins.
- `FrameDrive::on_demand` (the default): frames come only from UI work or from `FrameHook::request_frame()`. With nothing requested the app idles and presents nothing.

`FrameHook::set_drive` switches between the two at any time, which is how a player pauses: switch to on demand and request one last frame. `request_frame()` asks for exactly one frame. Both are safe from any thread and wake the UI loop.

## The contract

A hook must:

- Record only. Never submit, flush, finish, wait on a fence or otherwise block on the GPU from the CPU. arisa submits and presents.
- Bind every state it uses: render target, viewport, scissor, shaders, blend, rasteriser, depth and vertex state, uniforms. Assume nothing about what arisa or another hook left bound, and expect arisa to rebind its own state after the hook returns.
- Draw its whole content on every call. A hooked frame repaints the full target, and on a replay frame the hook is the only thing that redraws its content.
- Not touch the Root, the element tree, `Observable`s or any other UI state. A hook sees only the GPU context and `FrameInfo`. Pass values into it through atomics or the app's own thread-safe types, and send results back with `Dispatcher::post` or `LatestValue`.
- Not destroy its own `FrameHook`.

## Threading

Hooks run on the thread that submits frames. On deko3d, Vulkan and the null GPU that is the render thread (`ThreadRole::render`, core 1 on the console): it records, submits and presents every frame the UI thread painted, from the frame packet the UI thread handed it, and builds and presents the replay frames itself. On SDL_GPU, or with `ApplicationOptions::render_thread` off, frames are submitted inline on the UI thread. Submissions are serialised, so a hook never runs concurrently with itself or with another hook of the same Root.

Everything a hook sees comes from the frame packet: `FrameInfo`'s serial, predicted present, damage and replay flag were fixed when the UI thread sealed the frame, and surface rectangles and the publications to wait on were resolved then too. Nothing is read from the tree or from live UI state at submit, which is why a hook that only records, binds its own state and reads `FrameInfo` does not care which thread calls it.

## Transparent UI

The clear happens before the `before_ui` hooks, so content drawn there sits under the UI and the clear colour never covers it. What does cover it is any opaque UI: a root view, scaffold, navigation container or presentation panel that paints the theme background. A root built from stacks without a background (as `SampleScreen` and `GpuStageScreen` are) is already transparent. The built-in containers that paint an opaque full-area background are made transparent with `container_background` (see Container backgrounds below), so hook content and base-layer content show through a Scaffold, a NavigationStack, a split view's sidebar column or a sheet as well.

## Known limitation: glass over hook content

Glass and materials blur what arisa itself drew beneath them, starting from the clear colour. They do not sample hook content. Glass over a `before_ui` animation therefore shows a blurred clear colour, not a blurred animation: in the sample and the gpu stage the sidebar reads as a frosted panel in the clear colour, and its pixels do not change while the animation under it moves. Sampling hook content in the backdrop is planned for a later milestone. Content that arisa draws itself, such as an external texture shown with `Image::External` or a texture-mode `SurfaceView`, is part of the backdrop, so glass over it does blur it (the sample's card captions). To have full-screen app content under glass, put it in the base layer (below) rather than in a hook.

## The base layer

Every Root has an overlay layer below the main tree: `OverlayLayer::base`, at z 0, under `content` at 100. Whatever is placed there is drawn by arisa before any of the main tree, so it sits under the whole UI, including chrome, sheets and dialogs, and it is part of the backdrop that glass and materials above it blur and refract. That is the difference from a `before_ui` hook: a hook draws under the UI but outside arisa's backdrop, while the base layer draws under the UI and inside it.

The base layer is filled with SwiftUI's container background API, using the `window` placement, which in SwiftUI is the background behind all of a window's content:

- `.container_background(ContainerBackgroundPlacement::window, content)`, or with an alignment before the content, places a view in the base layer. `content` is a callable returning the view (SwiftUI's `@ViewBuilder` closure). A full-screen video is `.container_background(ContainerBackgroundPlacement::window, [frame] { return Image::External(frame).frame({.max_width = infinity, .max_height = infinity}); })`. Since M9 the same view form works for the navigation, navigation split view, scaffold and sheet placements: the view is drawn behind that container's content, clipped to its shape and laid out over its whole area at the given alignment.
- `.container_background(style, ContainerBackgroundPlacement::window)` fills the base layer with a colour, material or glass.

The layer is laid out over the whole screen and ignores the safe area. It takes no input: nothing in it is hit-tested or focusable, so taps and focus go to the UI above. It also works through the general layer API, `.overlay(is_presented, OverlayLayer::base, content)`, which gives the same placement.

The view keeps the declaring node's state and environment, like any layer, and it is shown while the declaring node is live, is not under a covered navigation destination and is not concealed. Unlike ordinary content it is not culled or occluded by what is drawn above it. Several base-layer views draw in declaration order. The `examples/gpu_stage` Video page has an "Under UI" button that moves its video from the card into the base layer, under the glass sidebar.

## Exclusive frames

An exclusive frame hands the screen to the app. While one is held, the main tree (`content`) and `chrome` are neither painted nor hit-tested, and no layer is drawn unless the frame keeps it; base-layer content and frame hooks are how the app fills the screen. It replaces borealis's `setExclusiveRender` and `setSuspendedRenderCallback`.

There are two ways to hold one, both scope objects:

- Imperatively, `root.exclusive_frame({.keep = {OverlayLayer::toast, OverlayLayer::hud}})` (or the same call on `Application`) returns an `ExclusiveFrame`. Destroying it, or calling `release()`, ends the scope. A frame that outlives its Root is harmless; releasing it does nothing.
- Declaratively, `.exclusive_frame(is_presented, keep)` on any element, with a `bool` or a `StateHandle<bool>`. The node holds the `ExclusiveFrame` in its own state while the value is true, and releases it when the value turns false, when the node starts to depart, or when it retires. Changing the keep list replaces the scope.

The keep list names the layers that still show: for a video player, typically `toast` and `hud`, and `dialog` if it should be able to ask something. `base` can be kept too. An empty list keeps nothing.

While exclusive:

- Only kept layers are painted. With nothing kept, the UI records no quads at all, and a frame is just the clear and the frame hooks.
- Only kept layers are hit-tested and focusable. Focus leaves the main tree; it moves to something focusable in a kept layer, or to nothing. Button commands then resolve from the node that holds the exclusive frame (the root for a `Root`-level frame), so `.on_exit_command` next to the `.exclusive_frame` modifier is how B ends it.
- Everything else keeps running: posted tasks, timers, node tasks, observation, reconciling and layout, and input routing. State changes in the hidden tree are applied, just not drawn.
- Frames are driven only by the hooks, by changes and animations in kept layers, by focus changes, resizes and lifecycle events. Changes in the hidden tree and raw input do not present anything, so an idle exclusive frame with an on-demand hook presents nothing at all, and a continuous hook sets the rate.

On release, focus returns to the node that was focused when the scope began, if it still exists, and the next frame repaints every swapchain image whole once. The hidden tree is not rebuilt: nothing is re-evaluated and layout is not recomputed, the release frame records the tree's paint again from the retained nodes, and from the frame after it retained paint splices unchanged subtrees as before.

Scopes nest and may overlap in any order. The rule is the intersection: a layer shows only if every live scope keeps it, so each scope's limit holds no matter what else is held, and releasing one widens what shows to what the remaining scopes allow. Focus restoration follows the scopes: releasing the newest scope restores the focus it saved, and releasing an older one hands its saved focus to the scope that began after it, so the focus from before the whole chain is restored when the last scope ends.

The exclusive state lives in the Root's tree and is read while painting; everything the presenter needs is in what paint produces. Frame hooks never see it and never touch the tree. The frame loop has an Exclusive state beside Waiting and Animating: entering or leaving is itself a reason to draw, and `FrameTimings::exclusive` reports it.

The `examples/gpu_stage` "Full screen" button holds an exclusive frame that keeps `toast` and `hud`: the animation fills the screen with a small HUD and a toast over it, and B releases it.

### Leasing the queue

`Options::lease` (`application.exclusive_frame({.keep = {}, .lease = true})`) makes the exclusive frame park arisa on the render queue: `Application::exclusive_frame` takes `RenderQueue::park()` before it returns, which waits for any submission the render thread has in flight, and from then on arisa builds, submits and replays nothing (`FrameTimings::leased`) until the scope is released. The app owns the queue and the display for that time, for example an emulator presenting its own frames on arisa's device. Its own submissions still take `RenderQueue::lease()`, which a park does not block. Releasing the scope gives the park back, repaints the UI whole and resumes normal frames. A declarative `.exclusive_frame` cannot lease; arisa parks at the next `frame()` when a leased scope comes from elsewhere. On SDL_GPU, where frames are submitted inline, the park is taken and released on the UI thread.

## The render queue

`GpuProvider::render_queue()` returns the `RenderQueue` of every provider that presents (deko3d, Vulkan, SDL_GPU, the null GPU). arisa takes `submit()` around each frame's queue work, before its own submit lock (deko3d and Vulkan: the whole frame, from the slot's fence wait through the present and any capture copy, and `capture()`; SDL_GPU and the null GPU: the whole submission). `submit()` returns no lease when the waiting submission is withdrawn: `withdraw()` returns a guard during which a submission that would wait is refused instead, which `Application` holds while it stops its render thread so a takeover cannot hang its destruction. An app takes `lease()` (or `try_lease()`) around its own work on the same queue; leases and arisa's submissions exclude each other, and `park()` (what a leased exclusive frame holds) excludes only arisa. `counters()` reports leases, parks, submissions, the waits on each side and `overlaps`, which stays 0; the render thread tests drive a leasing thread against frames and replays and check it.

## Taking over the GPU

A frame hook draws inside arisa's frame, on arisa's render thread, at arisa's swap interval. A video player or an emulator that wants to present on its own thread, with its own pacing and swap interval 0, takes the GPU over instead. arisa still owns the device; the takeover lends it.

`Deko3dGpu::take_over()`, `VulkanGpu::take_over()` and `NullGpu::take_over()` return `std::expected<Takeover, ProviderError>`. The `Takeover` is a move-only RAII handle built on `provider::GpuTakeover`:

- Taking it parks arisa on the render queue (`RenderQueue::park()`), which waits for any submission arisa has in flight. From then on arisa's render thread builds, submits and replays nothing, and `Application` paints nothing (`FrameTimings::taken_over`). Input, timers, posted work, observation, layout and the `Root` keep running, so the main tree can hold an `ExclusiveFrame` and an `InputBlock` and still react to B.
- Only one takeover exists at a time; a second `take_over()` fails with `ProviderErrorDetail::taken_over` (`takeover_counters()` counts grants, returns and refusals).
- The handle lends the device, the queue, the swapchain images and the means to acquire and present them. On deko3d: `device()`, `queue()`, `memory()` (each optional, empty once the handle is released), `image_count()` (0 once released), `target(index)` (a `FrameTarget`), `acquire()`, `present(index)`. On Vulkan: `device()` (optional; submit with `device()->get().with_queue(...)`), `image_count()`, `acquire(semaphore)` returning a `LentImage` (the `FrameTarget` with the image's current layout, whether the acquire semaphore is signalled, the `finished` semaphore your last submission must signal, and the layout to present in), and `present(index)`. `set_swap_interval(SwapInterval)` takes 0 to 4; 0 presents without waiting for vsync. `presents()` counts the app's presents.
- Every call on the handle is safe from any one thread, including a thread that is not the UI thread: akira presents from its Takion presenter thread. arisa does not touch the queue, the swapchain or the window while the handle is held.
- Each backend tracks the images the app has acquired (`provider::LentSwapchain`). `present(index)` of an image that is not acquired fails with `invalid_argument`, and `acquire()` fails with `image_unavailable` once every image is acquired instead of blocking; the null GPU follows the same contract, so host tests see what the consoles do.
- Destroying the handle (or `release()`) hands back: arisa first settles every image still acquired (deko3d presents it; Vulkan submits an empty batch that waits on the stored acquire semaphore and signals the image's `finished` semaphore, presents it, and rebuilds the swapchain at the next frame since the image's layout is unknown), so its own next acquire cannot block. It then waits for the queue to go idle (deko3d `waitIdle`, Vulkan `vkQueueWaitIdle`), restores its own swap interval (on Vulkan a change between 0 and non-zero rebuilds the swapchain in the matching present mode, `vulkan::PresentMode`), invalidates its retained per-image damage so the next frame repaints every image whole, releases the takeover seat before the park so a back-to-back claim is granted without arisa slipping a frame in, and wakes the UI loop. The first frame after the hand-back presents the whole UI; `Application` counts the queue's parks, so a takeover that began and ended between two of its frames still forces it.
- `Deko3dGpu::set_swap_interval` called during a takeover does not override the app's interval: it is kept and applied at the hand-back.
- A frame the render thread was about to submit when the park landed waits for the hand-back holding only the render-queue wait, not the provider's submit lock, so `set_swap_interval()` and the lockstep submit (stepped clocks) return at once; `capture()` refuses with `taken_over` while parked. `Application::drain()` waits for that frame, so it returns only after the hand-back. Destroying the `Application` while a takeover is alive withdraws the waiting submission and returns.

Present every image you acquire, in the layout the `LentImage` names on Vulkan. On Vulkan the frame's `vkQueueSubmit` now takes the render-queue lease from before the acquire, so a frame that was being built when the takeover began never holds a swapchain image while it waits. Take the handle from any thread except arisa's render thread, and never inside a frame hook.

The frame-hook path is unchanged and keeps working; an app moves from hooks to a takeover when it wants its own present.

### Recording a Root into the app's frame

While it holds a takeover, an app can have arisa draw a secondary `Root` (akira's stream overlay: the dock and its panels) into the app's own frame, after the app's video pass, with no second present, no hop to the UI thread and no copy of the frame. The pieces:

- `Takeover::overlay()` returns `std::expected<Overlay, ProviderError>` on deko3d/drako3d, Vulkan and the null GPU. One overlay exists per takeover; a second fails with `ProviderErrorDetail::overlay_open`. The `Overlay` is move-only; destroying it (or `release()`) closes it.
- `Overlay::frames()` returns a `provider::FrameSource`: a move-only class whose `open()` returns `std::expected<GpuFrame, ProviderError>` from the overlay's own ring of three slots, plus `closed()`, `uses()` (how many times the app has recorded a scene) and `await(after, limit)` (blocks until `uses()` passes `after`, the overlay closes, or `limit` runs out). The ring is separate from the main frame slots (its own geometry, uniforms, staging and fence per slot), so the overlay never waits on or takes a slot from the main UI.
- `Application::embed(Root& overlay, FrameSource frames, SurfaceSize extent)` returns `std::expected<EmbeddedRoot, ApplicationError>` (`already_embedded` for a second one). While the move-only `EmbeddedRoot` is attached, every `Application::frame()` on the UI thread also runs the overlay `Root`: it routes the sampled input to it through its own `InputRouter` and input gate, publishes the environment (input modality, hints, glyphs, display, locale, catalogue, theme), reconciles, lays it out at `extent` with its own `PainterRenderer`, and when its revision, layout or routing asks for it paints it into an overlay frame and submits that frame. Submitting an overlay frame presents nothing: it seals the scene and makes it the overlay's current one.
- **Input while embedded.** The main `Root` gets no input and its input gate is not consulted: `block_input()` and `pass_input()` on the main `Root` (or on `Application`) have no effect until the embedding ends. Gate the overlay instead: `InputBlock` and `InputPassthrough` taken from the overlay `Root` apply, so a passthrough of buttons the app reads itself belongs on the overlay `Root`. Embedding and detaching both interrupt the main router (a half-made press, drag or touch is dropped), and the buttons held at that moment are swallowed by the side that takes over until they are released: the A press that closes the overlay never activates the main UI, and the press that opened it never activates the overlay.
- **Pacing.** An animating overlay is painted at the app's pace, not the UI loop's: after publishing a scene the UI thread paints the next animation frame only once the app has recorded (used) the newest scene, and meanwhile waits in `FrameSource::await`, at most one refresh period (`Application::idle_poll`, 16 ms) before it samples input again. A repaint asked for by input, state or layout is not held back. When no slot is free the frame is owed and retried on the next use or after one refresh period. With an app that stops recording, the loop rests at one wake per refresh period.
- **Detaching.** `EmbeddedRoot::release()` (or its destruction) detaches. It must run on the UI thread; from inside the overlay's own frame (an action of the overlay that drops its `EmbeddedRoot`) the detach is deferred to the end of that frame, so the overlay's router, renderer and tree stay valid while the frame finishes. If the overlay is still open, detaching publishes one empty scene, so `record` draws nothing stale afterwards (when no slot is free yet it is retried on the following frames). When the overlay closes underneath it (the takeover is handed back or the `Overlay` is released), the next `Application::frame()` detaches by itself and `attached()` turns false; main input returns at once and the loop stops waking for the overlay.
- **Extent.** The extent is fixed at `embed`. When the target size changes (docked to handheld and back, or a swapchain rebuilt at a new size), drop the `EmbeddedRoot` and embed again with the new extent; a scene painted at the old extent is refused by `record` with `ProviderErrorDetail::overlay_extent` (checked against the extent stored with the scene at publish, under the ring's lock, without counting a use). Re-embedding reuses the same `Overlay`.
- `Overlay::record(...)` runs on the app's thread, between its video pass and its submit, and draws the current scene into the app's command buffer. It loads the target, never clears it, and redraws the whole scene every call (the app overwrites the image each frame, so no damage is carried between frames). With nothing to draw it records nothing and returns at once: `false` on deko3d and the null GPU, `std::nullopt` on Vulkan.

Per backend:

- **deko3d and drako3d:** `record(dk::CmdBuf&, const FrameTarget&) -> std::expected<bool, ProviderError>` records straight into the app's command buffer (simpler than a `callList` of a separate list and equivalent: the overlay slot's memory, not a list, is what must outlive the GPU's use). The first record of a scene also records its texture uploads, the clears of new render targets, the waits on published external textures, its `RenderSurfaces` (app surfaces drawn into arisa's offscreen layers) and its offscreen passes (backdrops, blurs), and the uploads and passes of any scene published but never recorded before it; later records of the same scene draw only the screen pass. It returns `true` whenever it recorded anything at all, including a first record that carries only uploads or offscreen passes and no screen draw: every command buffer `record` touched must be submitted, and must be submitted before the `Takeover::present()` that follows, because the slot's fence is signalled by that present (inside the ring's lock, before the slot can be seen as presented, so a claim never waits on a stale signal). Per buffer, the order is: `record`, submit the buffer, then `Takeover::present()`. `Overlay::release()` and the `Overlay`'s destructor call `waitIdle` on the queue, so they must run on the presenter thread (the one that records, submits and presents), never on the UI thread while the presenter may submit. The overlay binds its own render target, shaders, descriptor sets and state: rebind yours after it. A default view of `target.image` is bound.
- **Vulkan:** `record(VkCommandBuffer, const FrameTarget&) -> std::expected<std::optional<OverlayFence>, ProviderError>`. Record outside any render pass instance, after your dynamic-rendering pass ends, with the target in `target.layout` (normally `COLOR_ATTACHMENT_OPTIMAL`); arisa transitions it, begins its own load-op render pass, draws, and leaves the image in `COLOR_ATTACHMENT_OPTIMAL`. Its framebuffers are cached per view, extent and `FrameTarget::generation`: a newer generation retires the older ones, at most eight are kept (the least recently used is retired first), and a retired framebuffer is destroyed only once the timeline has passed the last use that drew with it, so bump `generation` whenever the swapchain is rebuilt. `statistics()` reports the cached and retired counts. The target's format must be the swapchain format (`invalid_argument` otherwise). The returned `OverlayFence{semaphore, value}` is a timeline semaphore and value: add it to the signal list of the submit that carries the command buffer. Uploads, render-target clears, `RenderSurfaces` and offscreen passes are not recorded into your buffer: arisa records them on the UI thread when the scene is published and submits them on the slot's own command buffer and fence, through `VulkanDevice::submit` (the queue mutex), before your submit of that scene can happen; texture writes made on the UI thread outside an overlay frame while the overlay is open (the embedded painter's glyph atlas pages, made at `embed`, among them) are submitted ahead of the next overlay frame's own work, so they are in place before any scene that samples them. Never call `record` or `Overlay::release()` from inside `VulkanDevice::with_queue`: both take arisa's submit lock, and publishing holds that lock while it takes the queue mutex. Submit through `VulkanDevice::submit` or `with_queue` (outside those calls). `Overlay::release()` waits for the device to go idle; release it after the last command buffer `record` touched has been submitted. Requires timeline semaphores (`unsupported_capability` otherwise). Recording inside your own render pass is not supported: arisa's pipelines are built for its render pass, not for dynamic rendering.
- **Null GPU:** `record(FrameCanvas) -> std::expected<bool, ProviderError>` rasterises the scene over the canvas `Takeover::target()` returned, without clearing it, and `Takeover::present()` completes the slot. Like deko3d it returns `true` when a first record only applied uploads or offscreen passes. `statistics()` counts published and empty scenes, refused frames, records, idle records and first (fresh) records; `current_slot()` names the slot being recorded.

**Threading contract.** The UI thread (the one that runs `Application::frame()`) owns the overlay `Root`, `Application::embed` and the `EmbeddedRoot`, and is the only caller of the `FrameSource`. The app's thread calls `Overlay::record`, `Takeover::present` and, at the end, `Overlay::release()` or the takeover's release. The handoff is the frame packet's: the UI thread claims a slot whose last use has completed (deko3d: presented and its fence signalled; Vulkan: the slot's own fence signalled and the timeline at or past its last use value; null: presented), writes geometry and commands into it, seals it, and submits it, which publishes it under the ring's lock (`provider::OverlaySlots`); the app thread takes the current scene under the same lock and marks the slot used with a fresh value. A slot that goes back to the ring without being recorded (abandoned, an empty scene, or on Vulkan a scene superseded before the app recorded it) takes the newest use value as its last use, because its own submission carries a newer serial than the scenes still on the GPU: reusing it early would retire their textures and targets. A slot is never claimed while any recorded use can still be on the GPU, and the UI thread never waits for one: when no slot is free it skips the frame and paints again later (see Pacing). A scene that was published but superseded before the app recorded it is not lost: on deko3d and the null GPU its uploads and offscreen passes are recorded ahead of the next scene's first record (`OverlaySlots::Leftovers::carried`); on Vulkan they were already submitted at publish. Closing races are safe on Vulkan: a frame published after the overlay closed submits only its staged uploads (so textures it created keep a truthful layout), records no passes, fails with `not_taken_over`, and the overlay's slots wait for their own fences before they are destroyed. Releasing the overlay waits for the queue to go idle (deko3d `waitIdle`, Vulkan `vkDeviceWaitIdle`), so release it from the thread that submits to the queue, or release the takeover, whose hand-back closes the overlay after its own wait; either way a later `record` fails with `not_taken_over`, and the application detaches the embedding on its next frame.

Limits: the overlay's glass and materials blur the overlay's own content beneath them, not the app's video, because arisa never sees the video.

## Device options on Vulkan

`vulkan::DeviceRequest` takes `extensions` (required), `optional_extensions` (enabled when offered), core `features` and `vulkan13` features. A missing required extension or feature fails `VulkanDevice::create` with `ProviderErrorDetail::missing_extension` or `missing_feature`, and `VulkanDevice::unsupported(instance, request)` names each one. `VulkanDevice::has_extension`, `extensions()`, `features()` and `vulkan13()` report what was enabled. On the Switch 2, `horizon::GpuOptions::vulkan` carries the same request (`extensions`, `optional_extensions`, and `VulkanFeature::dynamic_rendering`, `synchronization2`, `maintenance4`), and the factory reports the missing names through diagnostics.

## Capturing a frame

`Application::request_capture()` marks the next frame the UI thread paints; its packet carries `FramePlan::capture`, the provider reads that frame back right after presenting it (on the render thread, from the offscreen target or the null GPU's raster; swapchain targets cannot be read back), and `Application::take_capture()` returns it as a `CapturedFrame` with the packet's frame number. It is exactly the requested frame whatever the render thread has presented since. `GpuProvider::capture()` stays as the synchronous test helper; on Vulkan and deko3d it now serialises with the render thread's submission instead of reading under the renderer lock.

## Swap interval and pacing

A frame whose rate hint (the faster of the UI's hinted animations and continuous hooks, `FrameTimings::frame_rate`) is 30 fps or less carries `FramePlan::swap_interval = 2`; any full-rate content brings it back to 1. deko3d applies it with `dkSwapchainSetSwapInterval`, Vulkan on ounce with `nwindowSetSwapInterval` on the default window (through `SwapchainTarget::Options::pace`), and both record it (`Deko3dGpu::swap_interval_changes`, `VulkanGpu::Statistics::present_interval`/`interval_changes`; the host's FIFO and offscreen targets only record it). SDL_GPU keeps skipping frames in software. Where the provider reports when frames reach the display (`FrameReceipt::displayed`: deko3d and Vulkan swapchains, the null GPU with `display(period)`), the UI thread waits at the start of a frame rather than in the middle: `Application::wait()` sleeps until `next_build()`, the predicted vblank minus the measured UI and record time plus a 2 ms margin, and the frame's `FrameInfo::predicted_present` is that vblank. Software skipping is then off, since the interval and the wait pace the frame. Hooks that advance by `predicted_present` see real vblank times on the console.

## Container backgrounds

`.container_background(style, placement)` is SwiftUI's `containerBackground(_:for:)`. The style is a colour, a material or glass, and `Color::clear` (or any fully transparent colour) removes the background entirely. The placements are:

- `ContainerBackgroundPlacement::navigation`: the background of each NavigationStack screen.
- `ContainerBackgroundPlacement::navigation_split_view`: the inline sidebar column of a NavigationSplitView.
- `ContainerBackgroundPlacement::scaffold`: the Scaffold's frame. This one is arisa's own, since SwiftUI has no Scaffold.
- `ContainerBackgroundPlacement::sheet`: sheets and full-screen covers. On a presentation's content it is the same as `presentation_background`.
- `ContainerBackgroundPlacement::window`: the base layer, above.

A container uses the background declared on the container itself or on any of its ancestors. NavigationStack also reads one declared on a screen's own content, as SwiftUI does, and a sheet or cover reads it only from its content. Without a declaration every container paints exactly what it painted before. The view form, which takes a callable returning a view instead of a style, works for every placement (since M9): for `window` the view goes into the base layer, and for the others it is drawn behind the container's content over its whole area, at the alignment given (centre by default), clipped to the container's shape and never hit-tested.

Built-in components with an opaque full-area background, and how each is routed:

| Component | Default | Placement |
|---|---|---|
| Scaffold frame (inline header) | theme background | `scaffold` |
| Scaffold frame (floating header) | none | `scaffold` (only when declared) |
| NavigationStack screen (inline bar) | theme background | `navigation` |
| NavigationStack screen (floating bar) | none | `navigation` (only when declared) |
| NavigationSplitView inline sidebar column | secondary system background | `navigation_split_view` |
| NavigationSplitView floating sidebar | glass panel, not opaque | not routed |
| Sheet panel | theme background with sheet glass | `sheet` or `presentation_background` |
| Full-screen cover panel | theme background | `sheet` or `presentation_background` |
| Alert, confirmation dialog, popover, toast | raised-surface cards, not full-area | `presentation_background` |
| TabView, List, Form, Section, cells | no full-area background | none needed |

The fades under floating bars (the scroll-edge gradients of Scaffold and of the floating sidebar header) are short gradients, not backgrounds, and keep the theme colour.

## Transparency rules

What shows at a pixel, from the top: the UI layers in z order, then the base layer, then whatever the `before_ui` hooks drew, then the clear colour. So for content to reach the screen:

- Content in the base layer shows wherever the main tree and the layers above it draw nothing opaque. Clear the containers that would cover it with `container_background(Color::clear, ...)`.
- Content from a `before_ui` hook additionally needs the base layer empty where it should show, and glass over it blurs only arisa's own drawing.
- `after_ui` hook content covers everything, the UI included.
- During an exclusive frame, the main tree and every layer that is not kept draw nothing, so base-layer content (if `base` is kept) and hook content show through everywhere except under kept layers.

## Inspecting GPU content

The inspector (docs/inspector.md) shows GPU content without any code in the app: its GPU panel lists the frame hooks with their drive, the external textures with their publication state, and the surfaces with their mode and drive, and its Timeline puts the hooks' GPU time (`FrameTimings::hooks_gpu`) under each frame on deko3d and Vulkan. The sample and the gpu stage attach it behind `ARISA_INSPECTOR`; open it with L+R+minus or F12.

## Timing

On deko3d and Vulkan the GPU time spent in hooks is measured with timestamps around each stage and reported as `FrameTimings::hooks_gpu`, separately from the UI's `interface_gpu`. The Switch frame report prints it as "gpu hooks". On Vulkan the timestamps live in a query pool per frame slot and are read once that slot's fence has signalled, so the figures describe the frame before last and never stall. The null GPU reports the CPU time of its hooks in the same field. SDL_GPU has no timestamp queries, so it reports none.

## External textures

A frame hook draws into arisa's command buffer. An external texture goes the other way: the app renders or decodes into an image it owns, and arisa samples that image in place as part of the UI. This is the path for video players, emulators that render to their own target, and any producer that runs on its own schedule or thread. Nothing is copied: arisa writes a descriptor (deko3d) or a registry entry (SDL_GPU) that points at the app's image.

### Importing

Each GPU provider has `import_texture(native, ImportDescription, released)`:

- `Deko3dGpu::import_texture(const dk::Image&, ...)` writes an image descriptor into a free slot of arisa's descriptor set, through the same path `create_texture` uses. The descriptor is built from a `dk::ImageView` of the app's image, with the view format set from the description and a swizzle where the format needs one. The `dk::Image` object itself is not kept; its memory must stay valid until arisa releases the import.
- `Sdl3Gpu::import_texture(SDL_GPUTexture*, ...)` takes a texture the app created on `Sdl3Gpu::device()` with `SDL_GPU_TEXTUREUSAGE_SAMPLER`. arisa only adds a registry entry and never releases the texture.
- `VulkanGpu::import_texture(VkImage, ...)` takes an image the app created on `VulkanGpu::device()` with `VK_IMAGE_USAGE_SAMPLED_BIT`. arisa creates one image view of it, with the view format taken from the description and a swizzle for `alpha8`, and one combined image sampler descriptor set; it never copies, transitions or destroys the image. Imports are counted by `VulkanGpu::Statistics::imports`. Both Vulkan overloads also come with a `VkImageLayout` before the `released` callback: `VulkanGpu::import_layout` (`SHADER_READ_ONLY_OPTIMAL`, the default) or `VulkanGpu::host_import_layout` (`GENERAL`), which is the layout arisa then writes into the descriptor. Any other layout is refused. `GENERAL` is for linear images that the CPU writes in place, below.
- `NullGpu::import_texture(std::shared_ptr<SoftwareRaster::Canvas>, ...)` shares a CPU canvas with the software rasteriser, so tests can drive the whole path headless. Imports are counted by `NullGpu::imports()`.

`ImportDescription` carries the width, height, `format`, `filter` and `premultiplied`. Premultiplied content is drawn with the layer shading, which divides the alpha back out; straight alpha is drawn like any other image. The optional `released` callback is a `support::ScopedCallback` that arisa runs once it no longer needs the app's image (see Lifetime); it is the natural place to free the image's memory.

The call returns an `ExternalTexture`, a move-only handle. `id()` is the `TextureId` to show with `Image::External(texture.id())`. Import is safe from any thread.

### Creating images on arisa's Vulkan device

`VulkanGpu::device()` returns the `VulkanDevice` arisa draws with. It exposes what an app needs to create images and memory and to submit its own work on the same device: `instance().handle()` (the `VkInstance`), `physical()` (the `VkPhysicalDevice`), `handle()` (the `VkDevice`), `queue_family()`, `capabilities()`, `supports(format, features)`, and `allocate(requirements, MemoryUse, MemoryTiling)`, which binds through arisa's sub-allocator into its 16 MiB blocks so app images do not count against the driver's allocation limit one by one. The public `VulkanImage` and `VulkanBuffer` classes wrap that for the common case: `VulkanImage::create(device, {width, height, format, usage, swizzle})` makes an optimally tiled image with a view, which is what the gpu stage's video rings use. `VulkanHostImage::create(device, {width, height, format})` makes a linearly tiled, sampled image bound to persistently mapped host-visible memory (`MemoryUse::upload`, so coherent memory where the device has it), refused where the format has no `SAMPLED_IMAGE` bit in `linearTilingFeatures`. `bytes()` is its subresource as the CPU sees it and `row_pitch()` the driver's row pitch from `vkGetImageSubresourceLayout`; `flush()` flushes it when the memory is not coherent. It starts in `VK_IMAGE_LAYOUT_PREINITIALIZED` (`VulkanHostImage::initial_layout`), so the CPU may fill it before its first use; the app then moves it once to `GENERAL` and imports it with `host_import_layout`. The CPU writes it only while its import is not `in_use()`, and publishes with no semaphore: a host write before `vkQueueSubmit` is visible to that submission, and arisa's next frame is submitted after the publish.

arisa has one queue. An app submits to it with `VulkanDevice::submit(batches, fence)`, or runs anything else that needs the queue (a present of its own, `vkQueueWaitIdle`) inside `VulkanDevice::with_queue(function)`; both hold the mutex arisa holds around its own submits and presents, so they are safe from any thread, including a decoder thread. Calling `vkQueueSubmit` on `queue()` directly races arisa and is not allowed.

The contract for an imported image:

- It was created on this `VkDevice`, with sampled usage, and stays in memory until arisa calls the import's `released` callback (or until the app has destroyed the handle and `in_use()` has turned false).
- It is used only on arisa's queue family (`queue_family()`), so no queue family ownership transfer is ever needed. An image owned by another family must be released to `queue_family()` by the app before it is published.
- Whenever arisa may sample it, it is in `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` (`VulkanGpu::import_layout`). The app's command buffer that writes it ends with a barrier into that layout with destination stage `FRAGMENT_SHADER` and access `SHADER_READ`, and the app moves it out of that layout only while it is neither shown nor `in_use()`.

### Publishing

`publish(ready)` tells arisa that a new frame is in the image. It is safe from any thread, including a decoder thread, and never blocks.

A publish wakes the UI loop for a paint-only frame. Nothing is evaluated and nothing is reconciled: the frame walks the retained scene, and the draw-list runs that sample the published texture are marked changed. Damage is the bounds of those runs, plus the glass and material surfaces whose backdrop overlaps them, under the same reader rule as any other change. The rest of the screen is neither redrawn nor uploaded.

What `ready` is depends on the backend:

- deko3d: `publish(dk::Fence)`. The next submitted frame queues a GPU-side wait on that fence (`dk::Queue::waitFence`) before it samples, and invalidates the image cache. The CPU never waits. Signal the fence on the queue that rendered the image, typically the app's own queue; `publish()` with no fence means the image is already complete.
- SDL_GPU: `publish()`. Ordering comes from submission order on the single device: submit the command buffer that writes the image before publishing it. Begin the app's render pass with `cycle = true`, so a write never races a frame still sampling the old contents.
- Vulkan: `publish(ReadySemaphore{semaphore, value})`. The semaphore is one the app signals when the image is ready; the next frame arisa submits waits on it at the fragment shader stage, GPU-side, and the CPU never waits. With a `value` it is a timeline semaphore and arisa waits for that value; several publishes of the same timeline semaphore before a frame collapse into one wait on the highest value. Without a value it is a binary semaphore, and arisa waits on it once per publish, so each such publish needs exactly one signal. Timeline semaphores are preferred and are enabled by arisa whenever the device has them (`capabilities().timeline_semaphores`): both NVK on Mesa 26.2.2 (`VK_KHR_timeline_semaphore` at line 209 and `timelineSemaphore` at line 445 of `nvk_physical_device.c`) and MoltenVK 1.4 have them. `publish()` with no semaphore relies on submission order on the shared queue: the app's barrier into `SHADER_READ_ONLY_OPTIMAL` orders its writes before arisa's later frame, as long as it was submitted (through `VulkanDevice::submit`) before the publish. Waits are counted by `VulkanGpu::Statistics::semaphore_waits`.
- NullGpu: `publish()`. There is no fence; the canvas is read when the frame rasterises.

### Lifetime

Every draw that samples an external texture records the serial of the frame that will present it. `in_use()` stays true until that frame has completed on the GPU, by the same `GpuSerial` completion that `Deko3dGpu::completed` reports. Completion is learnt as arisa presents: on deko3d when a frame slot is reused, on SDL_GPU from the frame fences polled at the next frame, on Vulkan from the frame fences polled when the next frame begins or when `VulkanGpu::completed` is called, and on the null GPU right after present unless `NullGpu::hold_completion(n)` holds it back by `n` frames for tests. If nothing presents, completion does not advance either. On Vulkan the `released` callbacks run outside arisa's own locks, so a callback may import again.

Destroying the handle does not release anything that is still in use. The descriptor slot, registry entry or Vulkan image view and descriptor set, and the `released` callback, are kept until the last frame that sampled the texture has completed, and are then released on whichever thread learns of the completion. A handle destroyed while idle releases at once. Because lifetimes follow serials and fences, not "the next UI frame", they stay correct across the render thread: a draw is marked with the serial of the packet that will present it when the UI thread records it, and a texture destroyed while a packet that uses it is still queued keeps its registry entry until that packet has been submitted (`TextureRegistry::open`/`close`), so the render thread never looks up a texture that has gone.

### A decoder ring

A video player imports a small ring of images once, for example three, and keeps the handles for as long as it plays:

1. Pick the next image in the ring. If it is still `in_use()`, skip this frame or publish the current image again, which asks for a frame and lets completion catch up.
2. Decode or render into it, then publish it with its fence.
3. Show it: post its id to the UI thread (a `Dispatcher::post`, or an `Observable` set on the UI thread) and give it to `Image::External`. Changing the id evaluates only the component that reads it.

Only ever rewrite an image that is neither shown nor `in_use()`. That rules out a single image for a producer that keeps publishing: every presented frame samples the image on screen, so while frames keep coming it never stops being `in_use()`. Even a producer that always shows one picture (an emulator, say) needs a ring of at least two; with three, as in the sample, the next image has almost always completed by the time it is wanted. Publishing an image that is already shown repaints it without evaluating anything, and moving to the next id evaluates only the component that reads it. The sample's picture is the short version of this ring on every backend, and the `examples/gpu_stage` Video page the long one; on Vulkan (`VulkanVideo` and `VulkanNv12Video`) each ring slot owns its images, a command pool and a fence, records its own command buffer, submits it through `VulkanDevice::submit` signalling the next value of one timeline semaphore, and publishes with that value.

### Formats and filters

Textures, created or imported, may be `rgba8`, `alpha8`, `bgra8` or `rgb565`. `bgra8` holds blue in the first byte. `rgb565` is a 16-bit little-endian word with red in the top five bits. On deko3d they map to `RGBA8_Unorm`, `R8_Unorm` with a swizzle, `BGRA8_Unorm` and `BGR565_Unorm` (deko3d names packed formats from the low bits); on SDL_GPU to `R8G8B8A8`, `B8G8R8A8` and `B5G6R5`, with `rgb565` uploads expanded to `R8G8B8A8` where the device cannot sample `B5G6R5`; on Vulkan to `R8G8B8A8_UNORM`, `R8_UNORM` with a swizzle, `B8G8R8A8_UNORM` and `R5G6B5_UNORM_PACK16`. A Vulkan import does not convert, so the app's image must have exactly that format; an `rgb565` import is refused where the device cannot sample `R5G6B5_UNORM_PACK16`, and an `alpha8` import where image view swizzles are unavailable (`VK_KHR_portability_subset` without `imageViewFormatSwizzle`). The software rasteriser converts every format to RGBA when the canvas is written. `r8` and `rg8` hold one and two unsigned bytes per texel and sample as (r, 0, 0, 1) and (r, g, 0, 1); they map to `R8_Unorm` and `RG8_Unorm` on deko3d and `R8` and `R8G8` on SDL_GPU, and are the plane formats of NV12 below. They can be created and uploaded like any other format. `nv12` itself is import-only: `create_texture` refuses it and `Texture::upload_size` has no size for it.

`SamplerFilter::linear` is the default; `SamplerFilter::nearest` keeps texels sharp, for pixel art and emulators shown at an integer scale. The filter is part of the texture description, so it applies to created textures too. deko3d keeps both samplers in its sampler set and picks one per texture; SDL_GPU binds the matching sampler; the software rasteriser fetches the nearest texel.

### Replay

A frame that shows an external texture stays replayable. The texture's quad travels in the replay scene like any other, so the render thread keeps sampling it in place, and a replay frame shows the latest published content.

### NV12

Hardware video decoders write YUV, not RGBA: FFmpeg's nvtegra and NVDEC paths on Switch, VideoToolbox on macOS and VAAPI on Linux all hand out NV12 surfaces. arisa imports those two planes as they are and converts to RGB in the image shader while it samples them, so there is no extra pass and no intermediate RGBA image.

An NV12 image has two planes:

- the luma plane, one byte of Y per pixel at the full image size (`r8`);
- the chroma plane, two interleaved bytes per texel, Cb then Cr, at half the width and half the height, rounded up for odd sizes (`rg8`).

Import them with the plane overload of `import_texture`, giving the planes in that order and an `ImportDescription` whose `format` is `TextureFormat::nv12`, whose `width` and `height` are the image (luma) size, and whose two trailing fields say how to decode:

- `colour_matrix`: `ColourMatrix::bt601`, `bt709` (the default) or `bt2020` (non-constant luminance). Each fixes the luma weights Kr and Kb: 0.299 and 0.114, 0.2126 and 0.0722, 0.2627 and 0.0593.
- `range`: `ColourRange::limited` (the default; Y codes 16 to 235, Cb and Cr 16 to 240) or `ColourRange::full` (all codes 0 to 255, as JPEG and most screen captures use).

The overloads are:

- `Deko3dGpu::import_texture(std::span<const dk::Image* const>, ...)`, one descriptor per plane in arisa's descriptor set, viewed as `R8_Unorm` and `RG8_Unorm`. deko3d cannot report an image's size, so the planes must match the description; arisa checks only the count and that none is null.
- `Sdl3Gpu::import_texture(std::span<SDL_GPUTexture* const>, ...)`, two registry entries, sampled through the shape pipeline's two sampler bindings. The textures must be `R8_UNORM` and `R8G8_UNORM` with sampler usage; as with deko3d only the count and null-ness are checked.
- `VulkanGpu::import_texture(std::span<const VkImage>, ...)`, either two images, `R8_UNORM` and `R8G8_UNORM`, or one image of format `VK_FORMAT_G8_B8R8_2PLANE_420_UNORM`. A single multi-planar image is viewed per plane, `R8_UNORM` with `VK_IMAGE_ASPECT_PLANE_0_BIT` and `R8G8_UNORM` with `VK_IMAGE_ASPECT_PLANE_1_BIT`, so it must be created with `VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT` (and without `DISJOINT`, so that a barrier with the colour aspect moves both planes). No `VkSamplerYcbcrConversion` is involved: the planes are sampled as ordinary textures and converted by the same `ShapeKind::yuv` shader path as on every other backend. NVK can sample that format per plane: its format features are the intersection of the R8 and R8G8 plane features and always include sampling (`nvk_get_image_format_features`, `nvk_image.c` lines 126 to 188, with sampling asserted at line 155), and every plane is laid out as its own image of the plane format (`nvk_image.c` lines 864 to 870 and 961 to 990), which a plane view addresses directly. `capabilities().multi_planar_nv12` reports whether the device samples the format. As with deko3d only the count and null-ness are checked.
- `NullGpu::import_texture(std::span<const std::shared_ptr<SoftwareRaster::Canvas>>, ...)`, two canvases whose sizes must match the plane sizes exactly, or the import is refused.

`ImportDescription::planes()` and `plane(index)` give the plane count and each plane's texture description, so an app can size its own images from the description. The single-image overloads refuse `nv12`.

One `ExternalTexture` and one `TextureId` still identify the whole image. `Image::External(id)` works unchanged; `publish`, the deko3d fence, `in_use` and the deferred release behave exactly as for a single image, and both planes are held and released together, once, with one `released` callback. `TexturePublications::find(id)` reports the chroma plane's id and the conversion for an NV12 image; the chroma id is not itself a publication.

The conversion, per pixel:

1. Sample Y from the luma plane at the pixel's texture coordinate, and Cb and Cr from the chroma plane at the sited coordinate below, both through the description's filter.
2. Remove the range: for limited, Y' = (Y - 16/255) x 255/219 and C' = (C - 128/255) x 255/224; for full, Y' = Y and C' = C - 128/255.
3. Apply the matrix: R = Y' + 2(1 - Kr) Cr', B = Y' + 2(1 - Kb) Cb', G = Y' - (2 Kb (1 - Kb) / Kg) Cb' - (2 Kr (1 - Kr) / Kg) Cr', with Kg = 1 - Kr - Kb, and clamp each to 0..1. For BT.709 that is 1.5748, 1.8556, 0.1873 and 0.4681.

The output is the gamma-encoded R'G'B' the stream carries; no transfer function is applied, as for any other texture.

Chroma uses the standard MPEG-2 ("left") siting that H.264 and HEVC decoders produce by default: each chroma sample is co-sited horizontally with the even luma column and centred vertically between two luma rows. With a luma plane W wide and H tall and a chroma plane Wc by Hc, chroma is sampled at u_c = (u W / 2 + 1/4) / Wc and v_c = (v H / 2) / Hc. With linear filtering this is bilinear upsampling with the right phase: luma column 0 takes chroma sample 0, column 1 takes the average of samples 0 and 1, column 2 takes sample 1. With nearest filtering each pixel takes the closest chroma sample, rounding a tie to the right. Edges clamp. Streams with centre or top-left siting decode with a quarter-texel horizontal shift, which is invisible on video.

The painter passes the conversion to the GPU per vertex: the image quad is drawn with `ShapeKind::yuv`, the luma plane bound as the main texture and the chroma plane as the detail texture, the range terms in `effect` and the four matrix weights in `detail`. `provider::YuvConversion` computes those terms and holds the C++ form of the same arithmetic, which the software rasteriser uses, so headless tests decode exactly as the shaders do.

Glass and materials over an NV12 image work like over any external texture: the backdrop they sample is the converted RGB.

### An NV12 decoder ring

The ring is the one above, with two planes per slot:

1. At start, allocate a few decoder surfaces (three or four, or the decoder's own pool size). For each, wrap its two planes as native images (on Switch, `dk::Image`s laid out over the nvtegra surface memory with `R8_Unorm` and `RG8_Unorm`; on desktop, the planes of a VideoToolbox or VAAPI frame mapped into SDL_GPU textures by the app) and import them once as `nv12`, with the matrix and range of the stream. FFmpeg reports those per frame as `AVFrame::colorspace` (`AVCOL_SPC_BT470BG` and `AVCOL_SPC_SMPTE170M` are BT.601, `AVCOL_SPC_BT709` is BT.709, `AVCOL_SPC_BT2020_NCL` is BT.2020) and `AVFrame::color_range` (`AVCOL_RANGE_MPEG` is limited, `AVCOL_RANGE_JPEG` is full); an unspecified stream is usually BT.709 limited for HD and BT.601 limited for SD.
2. For each decoded frame, find the slot whose surface the decoder returned. If its handle is still `in_use()`, hold the frame back or drop it, and publish the current slot again.
3. Publish the slot, with the fence that signals the end of decoding on deko3d, and show its id.
4. If the stream's matrix or range changes, import that slot again with the new description before publishing it; the old handle retires and releases its descriptors once the GPU is done with them.

Keep the decoder from reusing a surface while its slot is `in_use()`: hold a reference to the frame (an `AVFrame` reference, for example) until the import reports idle, and let the import's `released` callback drop the last one.

The `examples/gpu_stage` Video page has an NV12 mode on every backend: the app renders its plasma as Y and CbCr into two plane images per ring slot, encoded as BT.709 limited, and shows them through the NV12 import. Its matrix and range pickers change what the import declares, so choosing anything but BT.709 limited shows the wrong colours.

## SurfaceView

`SurfaceView` puts GPU content inside the layout, where a hook can only draw under or over the whole screen. The app registers a surface with its GPU provider and shows it with `components::SurfaceView`, which takes part in layout like any other view: it fills the space it is offered, and `.frame` sizes it.

### Registering a surface

Every GPU provider has `add_surface(SurfaceOptions, function)`, which returns `std::expected<GpuSurface, ProviderError>`:

- `Deko3dGpu::add_surface`, whose function receives `dk::CmdBuf&`, a `Deko3dGpu::FrameTarget` and a `SurfaceFrame`.
- `VulkanGpu::add_surface`, whose function receives arisa's `VkCommandBuffer`, a `VulkanGpu::FrameTarget` and a `SurfaceFrame`.
- `Sdl3Gpu::add_surface`, whose function receives a `Sdl3Gpu::FrameTarget` (command buffer, texture, format, width, height) and a `SurfaceFrame`.
- `NullGpu::add_surface`, whose function receives the canvas it should draw into (empty when the null GPU is not rasterising) and a `SurfaceFrame`. `NullGpu::surface_calls()` counts the calls, for tests.

`SurfaceOptions` carries the `mode` (`SurfaceMode::texture`, the default, or `SurfaceMode::underlay`), the `drive` and `frame_rate` with the same meaning as for hooks, the `filter` used to sample a texture-mode surface (`SamplerFilter::nearest` for an emulator at an integer scale) and `premultiplied`. An empty function is refused with `invalid_argument`. Every backend supports both modes; a backend that cannot do a mode refuses it with `unsupported_capability` rather than accepting it and drawing nothing.

`GpuSurface` is the registration, move-only like `FrameHook`: `id()`, `mode()`, `request_frame()`, `set_drive(drive)`, `drive()` and `active()`, all safe from any thread. Destroying it unregisters the function (waiting only for a call already in flight) and retires its GPU resources.

`SurfaceFrame` describes one call: `surface`, `mode`, a `FrameInfo` (serial, predicted present, the target's extent, replay and damage, as for hooks), the `viewport` to draw into and the `scissor` to clip to, both in the target's pixels.

### Showing it

There are two constructions:

- `SurfaceView(surface)` shows a surface the app owns. This is the form for a producer outside the UI, such as an emulator core that calls `request_frame()` from its own thread when it has a new frame.
- `SurfaceView(gpu, options, function)` registers the surface itself, the way SwiftUI's `Canvas` takes its renderer. The registration is created when the view first appears and kept in the view's own state; when the view is evaluated again the new function replaces the old one, the drive follows `options.drive` and one frame is requested; when the view leaves the tree the registration ends with it.

### Texture mode

arisa owns a render-target texture sized to the view's laid-out rect in pixels, so a 64 by 48 point view is 64 by 48 pixels at scale 1 and 80 by 60 at 1.25. Before the UI, and before the offscreen passes of glass and materials, the provider calls the function with that texture as the target and a viewport and scissor covering all of it. The UI then samples the texture like any image: it clips to scroll views and other clips, scrolls, fades and transforms with its view, takes part in retained paint and replay, and glass and materials over it refract it.

The function runs only when there is a reason to draw: on every presented frame while the drive is continuous, once after `request_frame()`, and once whenever the texture is new (the first frame and after a resize). Otherwise the texture keeps its last content. A request is a paint-only frame, like an external texture's publish: nothing is evaluated or laid out, and the damage is the view's rect plus the glass and material surfaces whose backdrop overlaps it.

When the view's size changes the next paint creates a texture of the new size and retires the old one by `GpuSerial`, so it is released only once the frames that sampled it have completed. The same happens when the last view showing the surface leaves the tree, even if the app keeps the `GpuSurface`: the next view to show it starts with a fresh texture.

Per backend, the target is:

- deko3d: the texture's `dk::Image` and view, `DkImageFormat_RGBA8_Unorm`. Bind it with `bindRenderTargets` and set the viewport and scissor; arisa records a fragment barrier with an image cache invalidate after the surfaces, so sampling sees the writes.
- Vulkan: the texture's image and view in `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` (`hook_layout`), `VK_FORMAT_R8G8B8A8_UNORM`, called outside any render pass. arisa moves it from `SHADER_READ_ONLY_OPTIMAL` before the call and back after, with barriers that cover every stage, so the function needs no synchronisation of its own, exactly as for a hook. `FrameTarget::index` is 0 and `FrameTarget::generation` changes whenever the texture is replaced, so a framebuffer cached by view and generation stays valid until a resize. The old image is released by serial, so keep a framebuffer for an old generation for a few frames rather than destroying it at once; `VulkanPlasmaProgram::target` keeps the last few. The function runs on the render thread outside arisa's renderer lock; it may read `VulkanGpu::completed` but must not submit or wait.
- SDL_GPU: the texture, created in the swapchain format with colour-target and sampler usage, called outside any render pass. Begin a render pass on it; `DONT_CARE` or `CLEAR` is fine because the viewport covers it all.
- NullGpu: the texture's canvas.

### Underlay mode

The app draws straight into the frame target inside the view's rect: no texture, no copy, nothing sampled or filtered, so every pixel lands exactly where the app put it. This is the mode for emulators.

The function is called at the view's place in the UI's draw order, not before the whole UI: everything painted earlier (the backgrounds behind the view) is already drawn, arisa clears the scissor rect to the frame's clear colour, which is the hole in its own background, and then calls the function with the frame target, a viewport equal to the view's pixel rect and a scissor equal to that rect clipped to the view's clip and the target. Whatever the UI draws after the view (glass, labels, sheets) covers it as usual. Drawing at the view's place rather than before the UI is what keeps an opaque container background from painting over the content, without punching holes in every background in the tree.

The rect is translated with the view, including retained-paint splices and replayed scrolls, and rounded to whole pixels. Nothing else is applied: no clip beyond the rectangular scissor, no transform beyond translation, no opacity. The function runs on every presented frame on which the view is drawn, because the frame target is redrawn each time; the drive only decides when arisa presents frames on its behalf, and `request_frame()` asks for one. The view's rect is part of the damage of every frame arisa paints, and a replay repaints the whole target.

Per backend:

- deko3d: the swapchain `FrameTarget`. arisa sets the scissor and clears before the call and rebinds its own target, viewport and scissor after it.
- Vulkan: the swapchain `FrameTarget` in `COLOR_ATTACHMENT_OPTIMAL`, called outside a render pass with the same barriers as a hook. Begin a render pass whose render area is the scissor: with `loadOp` `DONT_CARE` the render area is the only part whose contents become undefined.
- SDL_GPU: the frame's texture. arisa blits its clear colour into the scissor rect first; begin a render pass with `LOAD` and set the viewport and scissor from the `SurfaceFrame`.
- NullGpu: the frame canvas, with the rect already filled with the clear colour.

### Glass over a surface

Glass and materials refract a texture-mode surface, because it is part of arisa's own drawing. They do not see underlay content: the offscreen passes that build their backdrop clear the underlay's rect to the clear colour, so glass over an underlay shows a blurred clear colour there. Use texture mode for content that glass must refract (decision D4).

### When the function does not run

The function never runs for a view that is not drawn on that frame: a view culled off screen, scrolled outside its scroll view, clipped to nothing, faded to zero opacity or hidden by an exclusive frame. A texture-mode surface keeps its last content and draws again when it is shown. A continuous surface still keeps frames coming while its view is hidden, so switch it to on demand when the app knows it is off screen, as the gpu stage does when it leaves its Surface page. After the view has left the tree, or the `GpuSurface` has been destroyed, the function is never called again.

### Threads

Surface functions run on the thread that submits frames, like hooks: the render thread on deko3d, Vulkan and the null GPU, and the UI thread on SDL_GPU. Which surfaces are due and where each underlay sits are decided when the UI thread seals the frame, so a view that moves on the next UI frame never moves an underlay already queued. A surface's function never runs concurrently with itself. The hook contract applies unchanged: record only, bind every state, do not touch the tree.

### The gpu stage Surface page

`examples/gpu_stage` has a Surface page with two views side by side in a scroll view: the plasma as a texture-mode surface under a glass "Texture" capsule, and the same plasma as an underlay. The sidebar switches both between continuous and on demand, and on demand adds a "Next frame" button. `--page surface` opens it on the desktop.

## Hardware video on Switch 2

libounce's decoder (`switch2/video/decoder.h`) decodes H.264, VP8, VP9 and AV1 in the console's video engine into NV12 images it allocates itself. The gpu stage's Decoder mode on ounce plays a short H.264 clip through it and shows the frames with the NV12 import. Every part of it has only been compiled: nobody has run it on a Switch 2.

### The route, and why

Three routes were weighed against the sources: OunceVK is Mesa 26.2.2 NVK (git `791c34e`), read from upstream `nvk_physical_device.c` and `nvk_image.c` and from the symbols and disassembly of the SDK's `libouncevk.a` (`vulkan.o`) and `libounce.a` (`decoder.o`, `viddec.o`, `gsv.o`).

- **A, import the decoder's memory without a copy: not possible with libounce as it is.** Upstream NVK 26.2.2 has no `VK_EXT_external_memory_host` (the device extension table, `nvk_physical_device.c` lines 112 to 327, lists `KHR_external_memory` and `EXT_external_memory_dma_buf` at lines 139 and 245 and nothing for host pointers), but OunceVK adds it: `nvk_GetMemoryHostPointerPropertiesEXT` is defined, gated on a device flag that `nvkmd_nvgpu_native_device_prepare` always sets, and it imports through `nvkmd_nvgpu_native_host_mem_create` with a 4 KiB alignment and size granule (`tst #0xfff`, `nvkmd_mem_init(..., 0x1000)`), into the host-visible, cached, non-coherent memory type (`nvk_host_ptr_memory_type_bits`). Its check, `nvkmd_nvgpu_native_backing_importable`, calls `svcQueryMemory` and accepts a range only if its type is heap, code-mutable, module-code-mutable or alias memory, its attribute is 0, its permission is read-write and it has no IPC or device references; then `backing_register` sets memory attribute `0x20` with `svcUnknown80` and registers the range with `gsvGpuRegisterMemory`. The decoder's output pool is built the other way round: `videoDecoderCreate` allocates the backing itself (`aligned_alloc`), and `viddecImagePoolCreate` hands it to `gsvCreateNativeMemory`, which sets the same attribute `0x20` with `svcUnknown80` whenever the frame mode the root permitted has bit 1, and OunceVK asks for mode 2 (`gsvRegisterFrame(2, ...)` in `nvkmd_nvgpu_native_runtime_open`). So if the root grants what OunceVK asks, a frame's planes carry attribute `0x20` by the time they exist and OunceVK refuses them; if it grants a mode without bit 1, the attribute stays clear but the range is already registered with the service as native memory, and whether that leaves the IPC or device references the check also refuses cannot be told from the sources. The reverse order, importing a buffer into Vulkan first and building the decoder's pool on it, needs the lower-level `viddec` API instead of `videoDecoder*`, would register one range with two GSV clients that each clear the attribute on release, and has no evidence either way, so it was not attempted. Wrapping the block-linear output directly would need the decoder's GOB layout to match NVK's `nil` layout and the same import, and fails at the same check.
- **B, Vulkan Video decode in NVK: not available.** Upstream 26.2.2 enables none of `VK_KHR_video_queue`, `_decode_queue`, `_decode_h264` or `_decode_h265` (absent from the same table), and in `libouncevk.a` every `nvk_*VideoKHR` entry point (`nvk_CreateVideoSessionKHR`, `nvk_CmdDecodeVideoKHR`, `nvk_GetPhysicalDeviceVideoCapabilitiesKHR` and the rest) is an undefined weak symbol, so the dispatch table has no implementation behind them.
- **C, one CPU copy into linear images that arisa samples in place: taken.** `videoDecoderCopyFrame` writes the frame's planes, pitch-linear or block-linear, to any destination pointers and pitches. The destinations are a ring of `VulkanHostImage`s, so the copy lands directly in the memory the GPU samples: no staging buffer and no GPU copy. NVK samples linear images: `nil_format_supports_texturing` does not depend on tiling (`nvk_image.c` line 61), linear tiling only drops colour attachment and depth (lines 78 and 87), a linear image becomes a pitch-linear `nil` image (line 850), and its row pitch comes from `nvk_get_image_subresource_layout` (lines 1541 to 1568). On the Switch 2 every memory type is system memory (`nvk_physical_device.c` lines 1637 to 1652), so `MemoryUse::upload` picks the coherent, uncached type, which suits a sequential CPU write.

### Classes

They live in `examples/gpu_stage`, not in the provider. The decoder has never run, its API covers one codec and one route, and the provider has no media abstraction for nx or desktop to share; it can move into `arisa::provider::horizon::ounce` once it has run on a console.

- `H264AnnexB` splits an Annex-B elementary stream into access units with no demuxer. A unit starts at an access unit delimiter, SEI, SPS, PPS or NAL types 14 to 18 after a picture, or at a slice whose `first_mb_in_slice` is 0 after a picture; each NAL is rewritten after a four-byte start code, as the decoder wants. `parameter_sets()` holds every distinct SPS and PPS for `VideoDecoderConfig::codec_config`.
- `FrameDecoder` is the decoder interface: `submit(sample, timestamp)` answers `accepted` or `busy`, and `next_frame()` returns a `DecodedFrame` or nothing without waiting. `OunceFrameDecoder` implements it over `videoDecoderCreate`, `Submit`, `GetFrame`, `CopyFrame` and `ReleaseFrame` in pitch-linear mode, all with a zero timeout; the libounce `VideoDecoder` lives in a session shared by the decoder and its frames, closed when the last of them goes.
- `DecodedFrame` is one decoded picture. `copy_to(planes)` checks that the destination planes are large enough and copies into them; destroying the frame returns its image to the decoder, exactly once.
- `VulkanDecodedVideo` owns the decoder, the stream and a ring of three slots, each a luma `R8` and a chroma `R8G8` `VulkanHostImage` imported together as one NV12 texture with `host_import_layout`. `advance(now)` submits up to two samples, holds up to two decoded frames, paces them by timestamp against the time of the first one, drops a frame when the next is already due, copies the due frame into the next slot that is not `in_use()`, publishes it and returns its id. When there is nothing new it publishes the current slot again, which keeps frames coming. `statistics()` counts samples submitted, frames decoded, shown and dropped, ring waits and the CPU time of the copies.

### Lifetimes

- A decoder image is returned as soon as its frame has been copied, so the decoder never waits on the GPU; it holds at most two of its four images in frames waiting to be shown.
- A ring slot is rewritten only when its import is not `in_use()`, that is when the last frame that sampled it has completed, and never while it is the slot on screen. If the next slot is busy the frame waits and the current slot is published again.
- The import's views and descriptors are released after the last frame that sampled them; `~VulkanDecodedVideo` waits for the device before the host images go.
- The stream loops forever: samples are resubmitted from the start with timestamps that keep rising, and the clip starts with an IDR picture so the decoder needs no flush.

### The clip

`examples/gpu_stage/media/stage.h264` is 4 seconds of FFmpeg's `testsrc2` at 1280x720 and 30 frames per second, Main profile level 3.1, no B-frames, an IDR picture every 60 frames, access unit delimiters on, SEI removed, BT.709 limited range: 120 pictures in 790182 bytes, MD5 `4f64b98afecf8e5eafeb7cdd14afc5a3` with FFmpeg 9.0.1 and x264 0.165.3222. It was made with:

```
ffmpeg -f lavfi -i testsrc2=size=1280x720:rate=30:duration=4 -threads 1 \
  -c:v libx264 -preset veryslow -profile:v main -level:v 3.1 -pix_fmt yuv420p \
  -bf 0 -g 60 -x264-params aud=1:keyint=60:min-keyint=60:scenecut=0 \
  -b:v 1500k -maxrate 1500k -bufsize 3000k \
  -color_primaries bt709 -color_trc bt709 -colorspace bt709 -color_range tv \
  -bsf:v filter_units=remove_types=6 -f h264 stage.h264
```

Only the ounce gpu stage carries it: its NRO gets its own romfs, a copy of the shared one plus `arisa/video/stage.h264`.

### The Decoder mode

On ounce, if the clip loads and the decoder starts, the Video page shows a Decoder button beside RGBA and NV12. It plays the clip in the card (or under the UI) and shows a status line with the route, frames decoded and dropped, and the mean copy time, refreshed every 15 frames. A decoder failure is logged over nxlink, shown in the status line and switches the page back to RGBA. On nx and desktop the button never appears.

### What has run and what has not

On the host, against MoltenVK: the splitter on the sample and on a hand-built stream, `DecodedFrame` bounds and single release, linear host images in `GENERAL` sampled through the NV12 import (exact grey levels), and `VulkanDecodedVideo` driven by a fake `FrameDecoder` that produces NV12 frames through the same interface (160 frames shown, none dropped, decoder images released after each copy, late frames dropped). MoltenVK also lists `VK_EXT_external_memory_host`, but nothing here uses it. Never run: `OunceFrameDecoder` (every libounce call), `VulkanHostImage` and `GENERAL` imports on NVK, the romfs load of the clip, and the Decoder mode in the ounce NRO.
