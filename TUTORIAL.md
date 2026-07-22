# Multimonitor — Short Tutorial

Show a different camera, render target, or HUD on each secondary monitor in Unreal Engine 5.7.

## 1. Install

1. Copy this plugin into your project:

   `YourProject/Plugins/Multimonitor/`

   (`Multimonitor.uplugin` must sit in that folder.)

2. Open the project (allow compile / regenerate if prompted).
3. **Edit → Plugins** → enable **Multimonitor** → restart the editor.

## 2. Check your monitors

In any Blueprint (e.g. Level Blueprint):

1. **Get Game Instance** → **Get Subsystem** → **Multimonitor Subsystem**
2. Call **Get Monitor Info**

Note each monitor’s **Index**. Index `0` is usually the first display. The monitor that shows your main game window is skipped automatically.

## 3. Create a layout

1. Content Browser → **Add → Miscellaneous → Data Asset**
2. Choose **Multimonitor Layout**
3. Add one or more **Slots**:

### Camera on a second screen

| Property | Value |
|----------|--------|
| Monitor Index | `1` (or your secondary index) |
| Content Type | **Camera** |
| Camera Actor | leave empty in the asset for now (set at runtime), or assign later via **Set Slot Content** |
| Capture Resolution | `(0,0)` = match monitor size |
| Fullscreen | enabled |

### Render target on a screen

| Property | Value |
|----------|--------|
| Monitor Index | secondary index |
| Content Type | **Render Target** |
| Render Target | your `TextureRenderTarget2D` asset |

### HUD / UMG on a screen

| Property | Value |
|----------|--------|
| Monitor Index | secondary index |
| Content Type | **HUD** |
| HUD Widget Class | your `User Widget` Blueprint class |

## 4. Apply at runtime

**Level Blueprint** (or Game Instance) **Event BeginPlay**:

1. Get **Multimonitor Subsystem**
2. **Apply Layout** → pass your `MultimonitorLayout` asset

Secondary windows open on the assigned monitors.

### Setting a camera from the level

If the layout’s Camera Actor is empty, after BeginPlay:

1. Build a **Multimonitor Slot** (Make struct)
2. Set **Content Type** = Camera, **Monitor Index**, **Camera Actor** = your `CameraActor` in the level
3. Call **Set Slot Content**

## 5. Stop / clean up

Call **Clear Layout** when you want the extra windows closed (e.g. EndPlay).  
They also close when the game instance shuts down.

## Minimal example flow

```
BeginPlay
  → Get Multimonitor Subsystem
  → Apply Layout (DA_MyLayout)
  → (optional) Set Slot Content for CameraActor reference
```

## Tips

- **Do not use the monitor that hosts Play-in-Editor** — that index is skipped so PIE is not covered. Call **Get Monitor Info** and use the other indices (with 3 displays you typically get two free outputs).
- Your OS “primary” monitor index is often **not** `0` (depends on Windows layout). Use **Get Primary Monitor Index** / **Get Monitor Info**.
- Leave **Allow Primary Monitor** unchecked unless you intentionally want to cover the PIE/game display.
- Render Target can stay empty for Camera slots (the plugin creates one).
- Use **extended** desktop mode in Windows (not Duplicate).
- Test in **Standalone Game** if PIE is awkward with multiple windows.
- Camera slots create a SceneCapture; heavier scenes cost more GPU per extra monitor.
- Multiple extras use borderless windows (not exclusive fullscreen) so several can run at once.
