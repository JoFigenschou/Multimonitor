# Multimonitor

Unreal Engine **5.7** runtime plugin that shows different content on secondary monitors using independent Slate windows (not nDisplay, not a single spanning window).

Each monitor slot can display:

- **Camera** — `SceneCaptureComponent2D` synced to a camera/actor, shown full-screen
- **RenderTarget** — an existing `UTextureRenderTarget2D`
- **HUD** — a `UUserWidget` subclass as the full window content

The primary game viewport monitor is left alone by default.

## Install

1. Copy or clone this repository into your project as:

   `YourProject/Plugins/Multimonitor/`

   The folder must contain `Multimonitor.uplugin` at its root.

2. Right-click your `.uproject` → **Generate Visual Studio project files** (or open the project and let the editor rebuild).

3. Enable **Multimonitor** under **Edit → Plugins**, then restart the editor.

## Quick start

See **[TUTORIAL.md](TUTORIAL.md)** for a short step-by-step guide.

### 1. Create a layout asset

In the Content Browser: **Add → Miscellaneous → Data Asset** → choose `MultimonitorLayout`.

Add slots:

| Property | Notes |
|----------|--------|
| `MonitorIndex` | `0` = first monitor from display metrics |
| `ContentType` | `Camera`, `RenderTarget`, or `HUD` |
| `CameraActor` | Level camera/actor (best set at runtime via Blueprint) |
| `RenderTarget` | Asset or soft reference |
| `HUDWidgetClass` | Your UMG widget class |
| `bFullscreen` | Cover the whole monitor (default `true`) |
| `CaptureResolution` | `(0,0)` = match monitor native size |

### 2. Apply from Blueprint

On **BeginPlay** (Game Instance or Level Blueprint):

1. Get **Multimonitor** subsystem (`Get Game Instance` → `Get Subsystem` → `Multimonitor Subsystem`)
2. Optional: call **Get Monitor Info** to discover indices/names
3. Call **Apply Layout** with your data asset

Or build a `Multimonitor Slot` at runtime and call **Set Slot Content**.

### 3. Tear down

Call **Clear Layout** when leaving the level, or rely on subsystem `Deinitialize` (also runs when the game instance shuts down).

## Blueprint API

| Function | Purpose |
|----------|---------|
| `Get Monitor Count` | Number of attached monitors |
| `Get Monitor Info` | Name, resolution, desktop rect, primary flag |
| `Get Primary Monitor Index` | Primary display index |
| `Apply Layout` | Create/update all secondary windows from a layout asset |
| `Clear Layout` | Destroy secondary windows and captures |
| `Set Slot Content` | Create/update a single monitor slot |
| `Get Window For Monitor` | Access the `Multimonitor Window` object |

## Content notes

- **Camera**: if the actor has a `CameraComponent`, capture uses its transform and FOV; otherwise the actor transform is used. If no render target is set on the slot, one is created at capture resolution.
- **RenderTarget**: displayed via `UMultimonitorRenderTargetWidget` (full-bleed `UImage`). You can subclass it or call `Set Display Material` for a UI material that samples the RT (`RenderTarget` parameter by default).
- **HUD**: the widget is **not** added to the game viewport; it is hosted only in the secondary Slate window.

## Platform

- **Win64** runtime module
- Requires extended desktop (not duplicate mode)
- Best verified in a packaged or standalone game with multiple displays; PIE works when secondary monitors are available

## License

See repository license if present; otherwise all rights reserved by the author.
