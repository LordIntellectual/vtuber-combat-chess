# UI menus — presentation notes

Practical notes for Main Menu, Settings, and related overlays. Keep this updated when
dimming / parallax / hero-art behaviour changes.

## Full-screen darken (“veil”)

### What it is

A **full-viewport semi-transparent rectangle** drawn after the background art (or over
the 3D scene). It darkens everything behind the UI so panels/text stay readable and
attention sits on the menu.

Typical form (y-down ortho, fixed-function):

```cpp
// Black / near-black, alpha ~0.3–0.55
drawRect(0, 0, (float)screenW, (float)screenH, 0.f, 0.f, 0.f, 0.55f);
// or slightly tinted:
// drawRect(0, 0, w, h, 0.02f, 0.03f, 0.07f, 0.32f);
```

This is **separate** from:

| Layer | Purpose | Keep for hero art? |
|-------|---------|------------------|
| Full-screen veil | Dim whole image | **No** when art is the star |
| Menu **panel** rect (e.g. α≈0.88 behind buttons) | Local contrast under labels | Yes |
| Dialog/modal veil | Focus a popup (quit / host) | Yes |

### Main Menu — removed (2026-08-07)

**Symptom:** Hero artwork (`share/nca/ui/menu_background.png`) looked dull/dark even after
parallax framing was fixed.

**Cause:** In `MainMenu::drawParallaxScene`, after drawing the far art layer, a **32%**
full-screen darken was applied:

```cpp
// REMOVED — was dulling hero art across the whole viewport
drawRect(0, 0, (float)screenW, (float)screenH, 0.02f, 0.03f, 0.07f, 0.32f);
```

**Fix:** Delete that full-viewport rect for Main Menu. Leave the semi-transparent
**panel** in `drawUiContent` (centred button stack only). Commits: `windows` `7f068e7`,
`main` `13bb0e6`.

**Policy for Main Menu:** no full-screen veil over hero art. Local panel + button chrome
only. Settings / modals may still dim.

### How to remove a veil on another menu later

1. Search that menu’s `draw` path for a rect covering `(0,0)` → `(screenW, screenH)` with
   alpha in ~`0.25`–`0.6` (often right after clear or before the panel).
2. Confirm it is not the **panel** itself (panel is smaller: `panelX/Y/W/H`).
3. Remove or gate the full-screen call, e.g. only when a modal is open, or never when
   hero art is the backdrop.
4. Rebuild and check both art brightness and text contrast on the panel.

### Inventory (where veils still exist)

| Location | File | Approx. alpha | Intent |
|----------|------|---------------|--------|
| ~~Main Menu parallax stage~~ | `MainMenu.cxx` `drawParallaxScene` | ~~0.32~~ | **Removed** — keep bright hero art |
| Main Menu quit confirm | `MainMenu.cxx` `drawQuitConfirm` | 0.55 | Modal focus |
| Main Menu host dialog | `MainMenu.cxx` `drawHostDialog` | 0.45 | Modal focus |
| Settings menu | `SettingsMenu.cxx` `draw` | 0.55 | Focus settings over game/scene |
| Settings quit confirm | `SettingsMenu.cxx` `drawQuitConfirm` | 0.55 | Modal focus |
| Piece editor | `PieceEditor.cxx` | 0.55 | Focus editor |

Candidates if we ever want brighter art under a screen: **Settings** full-screen dim
(when opened from Main Menu over hero art — Settings currently replaces the menu draw
path; if Settings later sits over the same parallax stage, revisit that 0.55 veil).

## Main Menu parallax (2.5D)

Implemented in `MainMenu::drawParallaxScene` / `projectMouseToUi`:

1. UI laid out in screen pixels → transparent **FBO**.
2. **Ortho** composite (same y-down as layout — *not* shader lookAt / `glFrustum` planes):
   - Far: oversize background, larger pan.
   - Near: full-screen UI FBO, smaller pan.
3. Hits: `ui = mouse − uiLayerPan` so hover matches the near layer.

Do **not** reintroduce true-3D planes with `getLookAtMatrix` / `getPerspectiveProjMatrix`
via `glLoadMatrixf` — those matrices are for the shader path and caused off-centre
framing (menu upper-right, hits still centre).

## Related assets

- Hero art: `share/nca/ui/menu_background.png` (also under `local/share/…` for runs).
- Code: `upstream/src/UI/MainMenu.cxx`, `MainMenu.hxx`.
