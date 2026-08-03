# Final fix report: stable SDL2 menu-frame capture

## Result

`plat_video_menu_enter()` no longer reads an SDL renderer backbuffer whose
contents became undefined after `SDL_RenderPresent()`. On menu entry it now
uses the still-valid last game `screen_texture` and the recorded software or
hardware-scaling geometry to reconstruct that frame in the renderer
backbuffer, then reads RGB565 pixels before any subsequent present.

This is a menu-entry-only operation. It does not add a full-screen CPU
readback to game frames.

## Root cause and selected design

SDL does not preserve a defined backbuffer after `SDL_RenderPresent()`, so a
successful post-present `SDL_RenderReadPixels()` could return empty or stale
pixels. `screen_pixels` is not an equivalent source for hardware scaling or
XRGB8888 input.

The SDL2 path already retains the last uploaded game texture plus
`screen_use_hw_scaling`, `screen_src_rect`, and `screen_dst_rect`. The fix
reuses that stable GPU-side state once at menu entry:

1. require a live renderer and ready game texture;
2. clear the renderer backbuffer;
3. render the retained texture with the same source/destination geometry used
   by `fb_flip()` (or full-screen null rectangles for the software path);
4. read the reconstructed output as RGB565;
5. only later replace the texture with the menu RGB565 texture and present.

This path is format-independent at the input texture boundary, so both
RGB565 and XRGB8888 hardware-scaled textures are converted by the renderer to
the displayed output before readback.

## TDD evidence

RED: added
`test_sdl_menu_capture_rebuilds_stable_frame_before_readback`. It requires a
stable-texture reconstruction helper, the hardware-scale source/destination
rectangles, strict `SDL_RenderClear` -> `SDL_RenderCopy` -> readback ordering,
no `SDL_RenderPresent` in the capture helper, and use of that helper by menu
entry. The isolated test failed 1/1 against the previous direct post-present
readback implementation.

GREEN: added the minimal `plat_sdl_capture_menu_frame()` helper and routed
menu capture through it. The new test and the existing independent
readback/fallback-source test pass 2/2.

Mutation check: removing the reconstruction helper, omitting the ready
texture guard, dropping either hardware-scaling rectangle, moving readback
before the copy, or adding a present before readback makes the new contract
fail. The existing capture contract fails if readback targets
`screen_pixels`, if the dedicated destination pitch is lost, or if the
failure fallback/no-ROM/dimension branches are removed.

## Verification

- Focused capture contracts: 2/2 pass.
- Full Python discovery: 37/38 pass. The sole failure is the documented,
  pre-existing `test_mednafen_wswan_defaults` fixture failure because
  `wswan_frameskip` is absent.
- Pure C menu-layout test: compiled with MinGW GCC 9.2.0 using
  `-std=c99 -Wall -Wextra`; executable passed with exit code 0.
- `git diff --check`: pass before report creation.
- Generated test executable and Python cache directories were removed.

The capture-specific contracts cover readback failure fallback, a dedicated
destination pitch, exact output dimensions, no-ROM clearing, pre-present
ordering, and both full-output and recorded hardware-scaling geometry. The
retained SDL texture makes the same path applicable to hardware-scaled
RGB565 and XRGB8888 game frames.

## Risks and unavailable validation

The host has no `make`, `sdl2-config`, SDL2 development headers, or H150101
cross-toolchain, so an affected SDL2/H150101 compile and interactive device
test could not be run. Renderer readback remains a one-time menu-entry cost;
if clear, texture copy, or readback fails, the existing untouched
`screen_pixels` fallback keeps menu entry functional, though hardware-scaled
or XRGB content may then fall back to the last software buffer as before.
