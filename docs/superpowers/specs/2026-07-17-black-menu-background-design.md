# Black Menu Background Design

## Goal

When the in-game menu is opened, show menu text and selection state on a
solid black background. Do not capture, copy, scale, or darken the current
game frame to produce the menu background.

## Scope

- Change the full-screen in-game menu background.
- Preserve menu text, selection rendering, borders, and navigation.
- Preserve save-state screenshot loading and saving.
- Preserve the bottom FPS/CPU HUD.
- Apply the black background consistently to SDL1 and SDL2 paths.

## Design

On menu entry, clear the menu background source buffer instead of copying the
current framebuffer into it. The SDL2 path must not call
`SDL_RenderReadPixels` for menu entry.

The shared menu background preparation function clears `g_menubg_ptr` to
black instead of copying and darkening `g_menubg_src_ptr`. Existing menu
drawing then renders text and selection state over that black buffer.

Save-state preview rendering remains independent: when a valid save-state
screenshot is loaded, the existing preview path may still populate and darken
the alternate background.

## Verification

Per user request, do not compile or run the program. Verify statically that:

- menu entry performs no framebuffer readback or game-frame copy;
- the normal menu background buffer is cleared to zero;
- FPS/CPU HUD rendering is unchanged;
- unrelated staged changes remain intact.
