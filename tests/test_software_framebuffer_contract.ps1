$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$core = Get-Content -Raw (Join-Path $root "core.c")
$plat = Get-Content -Raw (Join-Path $root "plat_sdl.c")
$header = Get-Content -Raw (Join-Path $root "plat.h")
$direct = Get-Content -Raw (Join-Path $root "video_direct.c")

function Require-Pattern([string]$text, [string]$pattern, [string]$message) {
	if ($text -notmatch $pattern) {
		throw $message
	}
}

Require-Pattern $header "plat_video_frame_begin\s*\(" "platform frame-begin API is missing"
Require-Pattern $header "plat_video_get_software_framebuffer\s*\(" "software framebuffer API is missing"
Require-Pattern $header "plat_video_frame_is_direct\s*\(" "direct-frame match API is missing"
Require-Pattern $header "plat_video_frame_end\s*\(" "platform frame-end API is missing"
Require-Pattern $core "RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER" "Libretro framebuffer environment command is not wired"
Require-Pattern $core "(?s)plat_video_frame_begin\s*\(\s*\).*?retro_run\s*\(\s*\).*?plat_video_frame_end\s*\(\s*\)" "retro_run is not enclosed by the direct-frame lifecycle"
Require-Pattern $core "plat_video_frame_is_direct\s*\(\s*data\s*,\s*width\s*,\s*height\s*,\s*pitch\s*\)" "video callback does not recognize direct frames"
Require-Pattern $direct "VIDEO_DIRECT_REPEAT" "repeated exact direct callback is not classified"
Require-Pattern $direct "VIDEO_DIRECT_OFFERED_MISMATCH" "later callbacks using the offered pointer are not classified safely"
Require-Pattern $plat "result\s*!=\s*VIDEO_DIRECT_EXTERNAL" "offered-pointer callbacks can fall through to upload"
Require-Pattern $plat "direct_callback_pending" "repeated direct callbacks are not prevented from double-counting"
Require-Pattern $plat "SDL_GetRendererInfo\s*\(" "renderer capabilities are not queried"
Require-Pattern $plat "texture_formats" "native RGB565 support is not derived from renderer formats"
Require-Pattern $plat "SDL_LockTexture\s*\(" "streaming texture is not locked"
Require-Pattern $plat "SDL_UnlockTexture\s*\(" "streaming texture is not unlocked"
Require-Pattern $plat "access_flags\s*&\s*RETRO_MEMORY_ACCESS_WRITE" "write access is not required"
Require-Pattern $plat "video_get_pixel_format\s*\(\s*\)\s*!=\s*RETRO_PIXEL_FORMAT_RGB565" "XRGB8888 is not rejected"
Require-Pattern $plat "rotate_display" "software rotation is not rejected"
Require-Pattern $plat "msg\[0\]" "HUD frames are not excluded"
Require-Pattern $plat "direct_frames" "direct profile counter is missing"
Require-Pattern $plat "upload_frames" "upload profile counter is missing"
Require-Pattern $plat "if\s*\(\s*profile_is_enabled\s*\(\s*\)\s*\)\s*\r?\n\s*sdl_video_profile\.direct_frames\+\+" "direct counter is not profiling-gated"
Require-Pattern $plat "if\s*\(\s*profile_is_enabled\s*\(\s*\)\s*\)\s*\r?\n\s*sdl_video_profile\.upload_frames\+\+" "upload counter is not profiling-gated"
Require-Pattern $plat "video_direct_validity_offer\s*\(\s*&direct_validity\s*\)\s*;\s*\r?\n\s*framebuffer->data\s*=" "an offered texture is not invalidated before returning it to the core"
Require-Pattern $plat "video_direct_validity_(accept|upload)\s*\(\s*&direct_validity\s*\)" "successful direct/upload frames do not validate texture contents"
Require-Pattern $plat "void\s+plat_video_dupe\s*\([^)]*\)\s*\{\s*\r?\n#ifdef USE_SDL2\s*\r?\n\s*if\s*\(\s*!video_direct_validity_can_dupe\s*\(\s*&direct_validity\s*\)\s*\)\s*\r?\n\s*return\s*;" "duplicate frames are not suppressed after an unmatched framebuffer offer"
Require-Pattern $plat "gameplay_upload_pending\s*=\s*true\s*;" "software-scaled gameplay uploads are not tracked"

$flipBody = [regex]::Match($plat, "static\s+void\s+\*fb_flip\s*\(void\)\s*\{(?<body>.*?)\n\}\s*\r?\n\s*void\s+\*plat_prepare_screenshot", "Singleline")
if (-not $flipBody.Success) {
	throw "fb_flip function is missing"
}
Require-Pattern $flipBody.Groups["body"].Value "gameplay_upload_pending" "fb_flip does not distinguish gameplay from menu uploads"
Require-Pattern $flipBody.Groups["body"].Value "upload_frames\+\+" "fb_flip does not count software-scaled gameplay uploads"
Require-Pattern $flipBody.Groups["body"].Value "if\s*\(\s*gameplay_upload_pending\s*\)\s*\{\s*\r?\n\s*video_direct_validity_upload\s*\(\s*&direct_validity\s*\)" "menu-only upload must not validate an invalid gameplay texture"

$directBody = [regex]::Match($plat, "void\s+plat_video_process_direct\s*\([^)]*\)\s*\{(?<body>.*?)\n\}", "Singleline")
if (-not $directBody.Success) {
	throw "isolated direct-frame submission function is missing"
}
if ($directBody.Groups["body"].Value -match "SDL_UpdateTexture") {
	throw "direct-frame path must not upload the locked texture"
}

Write-Output "software framebuffer source contract: PASS"
