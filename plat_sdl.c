#include <SDL/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdint.h>
#include "core.h"
#include "libpicofe/fonts.h"
#include "libpicofe/plat.h"
#include "menu.h"
#include "options.h"
#include "plat.h"
#include "scale.h"
#include "util.h"

#ifdef USE_SDL2
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *screen_texture;
static SDL_Texture *screen_copy_texture;
static SDL_Rect screen_src_rect;
static SDL_Rect screen_dst_rect;
static Uint32 screen_tex_format;
static unsigned screen_tex_w;
static unsigned screen_tex_h;
static size_t screen_tex_pitch;
static bool screen_texture_ready;
static bool screen_texture_linear;
static bool screen_use_hw_scaling;
static bool screen_hw_msg_pending;
static bool screen_renderer_vsync;
static unsigned screen_clear_frames;
static bool screen_last_dst_rect_valid;
static SDL_Rect screen_last_dst_rect;
static unsigned screen_last_log_w;
static unsigned screen_last_log_h;
static size_t screen_last_log_pitch;
static uint16_t screen_pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
static SDL_Surface *screen;

struct sdl_video_profile {
	unsigned frames;
	uint64_t last_log_us;
	uint64_t update_us;
	uint64_t clear_us;
	uint64_t copy_us;
	uint64_t present_us;
	uint64_t pre_present_us;
	uint64_t readback_us;
	uint64_t hud_us;
	uint64_t audio_wait_us;
	uint64_t audio_wait_frame_us;
	uint64_t pace_us;
	uint32_t update_max_us;
	uint32_t clear_max_us;
	uint32_t copy_max_us;
	uint32_t present_max_us;
	uint32_t pre_present_max_us;
	uint32_t readback_max_us;
	uint32_t hud_max_us;
	uint32_t audio_wait_max_us;
	uint32_t audio_wait_frame_max_us;
	uint32_t pace_max_us;
	unsigned late_present_frames;
};

static struct sdl_video_profile sdl_video_profile;
static uint64_t sdl_flip_start_us;
#else
static SDL_Surface* screen;
#endif

struct audio_state {
	unsigned buf_w;
	unsigned max_buf_w;
	unsigned buf_r;
	size_t buf_len;
	struct audio_frame *buf;
	int in_sample_rate;
	int out_sample_rate;
	int sample_rate_adj;
	int adj_out_sample_rate;
};

struct audio_state audio;

static void plat_sound_select_resampler(void);
void (*plat_sound_write)(const struct audio_frame *data, int frames);

#define DRC_MAX_ADJUSTMENT 0.003
#define DRC_ADJ_BELOW 40
#define DRC_ADJ_ABOVE 60

static char msg[HUD_LEN];
static unsigned msg_priority = 0;
static unsigned msg_expire = 0;

static bool frame_dirty = false;
static int frame_time = 1000000 / 60;

#ifdef USE_SDL2
static void plat_sdl_log_video_drivers(void)
{
	int i, count = SDL_GetNumVideoDrivers();

	PA_ERROR("SDL video drivers:");
	for (i = 0; i < count; i++)
		PA_ERROR(" %s", SDL_GetVideoDriver(i));
	PA_ERROR("\n");
}

static void plat_sdl_set_platform_defaults(void)
{
#ifdef SF3000
	if (!getenv("SDL_VIDEODRIVER"))
		setenv("SDL_VIDEODRIVER", "fb_hcge", 0);
	if (!getenv("SDL_RENDER_DRIVER"))
		setenv("SDL_RENDER_DRIVER", "fb_hcge", 0);
	if (!getenv("SDL_AUDIODRIVER"))
		setenv("SDL_AUDIODRIVER", "alsa", 0);
#endif
}
#endif

static uint64_t plat_get_ticks_us_u64(void) {
	uint64_t ret;
	struct timeval tv;

	gettimeofday(&tv, NULL);

	ret = (uint64_t)tv.tv_sec * 1000000;
	ret += (uint64_t)tv.tv_usec;

	return ret;
}

#ifdef USE_SDL2
static void plat_sdl_profile_add(uint64_t *total_us, uint32_t *max_us,
				 uint64_t elapsed_us)
{
	*total_us += elapsed_us;
	if (elapsed_us > *max_us)
		*max_us = (elapsed_us > UINT32_MAX) ? UINT32_MAX : (uint32_t)elapsed_us;
}

static void plat_sdl_profile_frame(void)
{
	uint64_t now = plat_get_ticks_us_u64();
	uint64_t elapsed;

	if (!sdl_video_profile.last_log_us)
		sdl_video_profile.last_log_us = now;

	sdl_video_profile.frames++;
	elapsed = now - sdl_video_profile.last_log_us;
	if (elapsed >= 1000000 && sdl_video_profile.frames) {
		double frames = (double)sdl_video_profile.frames;
		double secs = (double)elapsed / 1000000.0;

		PA_INFO("PROFILE sdl: fps=%.1f update=%.2f/%u clear=%.2f/%u copy=%.2f/%u pre_present=%.2f/%u late=%u present=%.2f/%u readback=%.2f/%u hud=%.2f/%u audio_wait=%.2f/%u ms avg/max\n",
			frames / secs,
			(double)sdl_video_profile.update_us / frames / 1000.0,
			sdl_video_profile.update_max_us / 1000,
			(double)sdl_video_profile.clear_us / frames / 1000.0,
			sdl_video_profile.clear_max_us / 1000,
			(double)sdl_video_profile.copy_us / frames / 1000.0,
			sdl_video_profile.copy_max_us / 1000,
			(double)sdl_video_profile.pre_present_us / frames / 1000.0,
			sdl_video_profile.pre_present_max_us / 1000,
			sdl_video_profile.late_present_frames,
			(double)sdl_video_profile.present_us / frames / 1000.0,
			sdl_video_profile.present_max_us / 1000,
			(double)sdl_video_profile.readback_us / frames / 1000.0,
			sdl_video_profile.readback_max_us / 1000,
			(double)sdl_video_profile.hud_us / frames / 1000.0,
			sdl_video_profile.hud_max_us / 1000,
			(double)sdl_video_profile.audio_wait_us / frames / 1000.0,
			sdl_video_profile.audio_wait_max_us / 1000);
		PA_INFO("PROFILE sdl2: pace=%.2f/%u audio_wait_frame=%.2f/%u ms avg/max\n",
			(double)sdl_video_profile.pace_us / frames / 1000.0,
			sdl_video_profile.pace_max_us / 1000,
			(double)sdl_video_profile.audio_wait_us / frames / 1000.0,
			sdl_video_profile.audio_wait_frame_max_us / 1000);

		memset(&sdl_video_profile, 0, sizeof(sdl_video_profile));
		sdl_video_profile.last_log_us = now;
	}
}
#endif

static void video_expire_msg(void)
{
	msg[0] = '\0';
	msg_priority = 0;
	msg_expire = 0;
}

static void video_update_msg(void)
{
	if (msg[0] && msg_expire < plat_get_ticks_ms())
		video_expire_msg();
}

static void video_clear_msg(uint16_t *dst, uint32_t h, uint32_t pitch)
{
	memset(dst + (h - 10) * pitch, 0, 10 * pitch * sizeof(uint16_t));
}

static void video_print_msg(uint16_t *dst, uint32_t h, uint32_t pitch, char *msg)
{
	basic_text_out16_nf(dst, pitch, 2, h - 10, msg);
}

#ifdef USE_SDL2
static bool plat_sdl_is_hw_scale_supported(unsigned width, unsigned height, size_t pitch)
{
	if (rotate_display)
		return false;

	if (width == 0 || height == 0 || pitch == 0)
		return false;

	if (pitch != width * sizeof(uint16_t) &&
	    pitch != width * sizeof(uint32_t))
		return false;

	return true;
}

static Uint32 plat_sdl_texture_format_for_pitch(unsigned width, size_t pitch)
{
	return (pitch == width * sizeof(uint32_t)) ?
		SDL_PIXELFORMAT_XRGB8888 : SDL_PIXELFORMAT_RGB565;
}

static void plat_sdl_compute_hw_rects(unsigned width, unsigned height,
				      SDL_Rect *src, SDL_Rect *dst)
{
	double aspect = (double)width / (double)height;
	int src_x = 0;
	int src_y = 0;
	int src_w = (int)width;
	int src_h = (int)height;
	int dst_x = 0;
	int dst_y = 0;
	int dst_w = SCREEN_WIDTH;
	int dst_h = SCREEN_HEIGHT;

	if (width == 384 && height == 224) {
		aspect = 10.0 / 7.0;
	} else if (strstr(core_name, "pcsx")) {
		aspect = 4.0 / 3.0;
	} else if (strstr(core_name, "snes")) {
		aspect = 8.0 / 7.0;
	}

	switch (scale_size) {
	case SCALE_SIZE_NATIVE:
		dst_w = (int)width;
		dst_h = (int)height;
		dst_x = (SCREEN_WIDTH - dst_w) / 2;
		dst_y = (SCREEN_HEIGHT - dst_h) / 2;
		if (dst_y < 0) {
			src_h = (int)((double)height * (double)SCREEN_HEIGHT / (double)dst_h);
			if (src_h < 1)
				src_h = 1;
			src_y = ((int)height - src_h) / 2;
			dst_y = 0;
			dst_h = SCREEN_HEIGHT;
		}
		if (dst_x < 0) {
			src_w = (int)((double)width * (double)SCREEN_WIDTH / (double)dst_w);
			if (src_w < 1)
				src_w = 1;
			if (pan_display == PAN_DISPLAY_LEFT) {
				src_x = 0;
			} else if (pan_display == PAN_DISPLAY_RIGHT) {
				src_x = (int)width - src_w;
			} else {
				src_x = ((int)width - src_w) / 2;
			}
			dst_x = 0;
			dst_w = SCREEN_WIDTH;
		}
		break;
	case SCALE_SIZE_STRETCHED:
		dst_x = 0;
		dst_y = 0;
		dst_w = SCREEN_WIDTH;
		dst_h = SCREEN_HEIGHT;
		break;
	case SCALE_SIZE_SCALED:
		dst_w = SCREEN_WIDTH;
		dst_h = (int)lround((double)SCREEN_WIDTH / aspect);
		dst_x = 0;
		dst_y = (SCREEN_HEIGHT - dst_h) / 2;
		if (dst_h > SCREEN_HEIGHT) {
			dst_h = SCREEN_HEIGHT;
			dst_w = (int)lround((double)SCREEN_HEIGHT * aspect);
			dst_x = (SCREEN_WIDTH - dst_w) / 2;
			dst_y = 0;
		}
		break;
	case SCALE_SIZE_CROPPED:
	case SCALE_SIZE_MANUAL:
	{
		double zoom = (scale_size == SCALE_SIZE_CROPPED) ? 1.0 : ((double)zoom_level / 100.0);
		unsigned base_w = width;
		unsigned base_h = height;
		unsigned full_crop_h = SCREEN_HEIGHT;
		unsigned full_crop_w;

		if (width > 240) {
			base_w = SCREEN_WIDTH;
			base_h = (unsigned)lround((double)SCREEN_WIDTH / aspect);
			if (base_h > SCREEN_HEIGHT) {
				base_h = SCREEN_HEIGHT;
				base_w = (unsigned)lround((double)SCREEN_HEIGHT * aspect);
			}
		}

		full_crop_w = (unsigned)lround((double)full_crop_h * aspect);
		dst_w = (int)lround((double)base_w + ((double)full_crop_w - (double)base_w) * zoom);
		dst_h = (int)lround((double)base_h + ((double)full_crop_h - (double)base_h) * zoom);
		if (dst_w < 1)
			dst_w = 1;
		if (dst_h < 1)
			dst_h = 1;

		if (dst_w > SCREEN_WIDTH) {
			double src_pixels_per_virtual = (double)width / (double)dst_w;
			src_w = (int)lround((double)SCREEN_WIDTH * src_pixels_per_virtual);
			if (src_w < 1)
				src_w = 1;
			if (src_w > (int)width)
				src_w = (int)width;

			if (pan_display == PAN_DISPLAY_LEFT) {
				src_x = 0;
			} else if (pan_display == PAN_DISPLAY_RIGHT) {
				src_x = (int)width - src_w;
			} else {
				src_x = ((int)width - src_w) / 2;
			}

			dst_x = 0;
			dst_w = SCREEN_WIDTH;
		} else {
			dst_x = (SCREEN_WIDTH - dst_w) / 2;
		}

		if (dst_h > SCREEN_HEIGHT) {
			double src_pixels_per_virtual_y = (double)height / (double)dst_h;
			src_h = (int)lround((double)SCREEN_HEIGHT * src_pixels_per_virtual_y);
			if (src_h < 1)
				src_h = 1;
			if (src_h > (int)height)
				src_h = (int)height;
			src_y = ((int)height - src_h) / 2;
			dst_y = 0;
			dst_h = SCREEN_HEIGHT;
		} else {
			dst_y = (SCREEN_HEIGHT - dst_h) / 2;
		}
		break;
	}
	default:
		break;
	}

	if (src_x < 0)
		src_x = 0;
	if (src_y < 0)
		src_y = 0;
	if (src_x + src_w > (int)width)
		src_w = (int)width - src_x;
	if (src_y + src_h > (int)height)
		src_h = (int)height - src_y;
	if (dst_w < 1)
		dst_w = 1;
	if (dst_h < 1)
		dst_h = 1;

	src->x = src_x;
	src->y = src_y;
	src->w = src_w;
	src->h = src_h;
	dst->x = dst_x;
	dst->y = dst_y;
	dst->w = dst_w;
	dst->h = dst_h;
}

static void plat_sdl_destroy_screen_texture(void)
{
	if (screen_texture) {
		SDL_DestroyTexture(screen_texture);
		screen_texture = NULL;
	}
	screen_texture_ready = false;
	screen_tex_format = 0;
	screen_tex_w = 0;
	screen_tex_h = 0;
	screen_tex_pitch = 0;
}

static void plat_sdl_destroy_screen_copy_texture(void)
{
	if (screen_copy_texture) {
		SDL_DestroyTexture(screen_copy_texture);
		screen_copy_texture = NULL;
	}
}

static int plat_sdl_ensure_screen_texture(unsigned width, unsigned height,
					  size_t pitch, Uint32 format, bool linear)
{
	if (screen_texture &&
	    screen_tex_format == format &&
	    screen_tex_w == width &&
	    screen_tex_h == height &&
	    screen_tex_pitch == pitch &&
	    screen_texture_linear == linear) {
		return 0;
	}

	plat_sdl_destroy_screen_texture();

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, linear ? "linear" : "nearest");
	screen_texture = SDL_CreateTexture(renderer,
					   format,
					   SDL_TEXTUREACCESS_STREAMING,
					   width,
					   height);
	if (!screen_texture) {
		PA_ERROR("%s, failed to create screen texture %ux%u: %s\n",
			 __func__, width, height, SDL_GetError());
		return -1;
	}

	screen_tex_format = format;
	screen_tex_w = width;
	screen_tex_h = height;
	screen_tex_pitch = pitch;
	screen_texture_linear = linear;
	screen_texture_ready = true;
	return 0;
}

static int plat_sdl_ensure_screen_copy_texture(void)
{
	if (screen_copy_texture)
		return 0;

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	screen_copy_texture = SDL_CreateTexture(renderer,
						SDL_PIXELFORMAT_RGB565,
						SDL_TEXTUREACCESS_STREAMING,
						SCREEN_WIDTH,
						SCREEN_HEIGHT);
	if (!screen_copy_texture) {
		PA_ERROR("%s, failed to create screen copy texture: %s\n",
			 __func__, SDL_GetError());
		return -1;
	}

	return 0;
}

static void plat_sdl_readback_screen(void)
{
	uint64_t start_us;

	if (!renderer)
		return;

	start_us = plat_get_ticks_us_u64();
	if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGB565,
				 screen_pixels, SCREEN_PITCH) != 0) {
		PA_WARN("%s, SDL_RenderReadPixels failed: %s\n",
			__func__, SDL_GetError());
	}
	plat_sdl_profile_add(&sdl_video_profile.readback_us,
			     &sdl_video_profile.readback_max_us,
			     plat_get_ticks_us_u64() - start_us);
}
#endif

static int audio_resample_passthrough(struct audio_frame data) {
	audio.buf[audio.buf_w++] = data;
	if (audio.buf_w >= audio.buf_len) audio.buf_w = 0;

	return 1;
}

static int audio_resample_nearest(struct audio_frame data) {
	static int diff = 0;
	int consumed = 0;

	if (diff < audio.adj_out_sample_rate) {
		audio.buf[audio.buf_w++] = data;
		if (audio.buf_w >= audio.buf_len) audio.buf_w = 0;

		diff += audio.in_sample_rate;
	}

	if (diff >= audio.adj_out_sample_rate) {
		consumed++;
		diff -= audio.adj_out_sample_rate;
	}

	return consumed;
}

static void *fb_flip(void)
{
#ifdef USE_SDL2
	uint64_t start_us;

	if (!screen_texture_ready)
		return screen_pixels;

	if (!screen_use_hw_scaling) {
		start_us = plat_get_ticks_us_u64();
		SDL_UpdateTexture(screen_texture, NULL, screen_pixels, SCREEN_PITCH);
		plat_sdl_profile_add(&sdl_video_profile.update_us,
				     &sdl_video_profile.update_max_us,
				     plat_get_ticks_us_u64() - start_us);
	}

	if (need_full_clear)
		screen_clear_frames = 3;

	if (!screen_use_hw_scaling || screen_clear_frames > 0) {
		start_us = plat_get_ticks_us_u64();
		SDL_RenderClear(renderer);
		plat_sdl_profile_add(&sdl_video_profile.clear_us,
				     &sdl_video_profile.clear_max_us,
				     plat_get_ticks_us_u64() - start_us);
		if (screen_clear_frames > 0)
			screen_clear_frames--;
		need_full_clear = 0;
	}

	start_us = plat_get_ticks_us_u64();
	if (screen_hw_msg_pending) {
		SDL_RenderCopy(renderer, screen_copy_texture, NULL, NULL);
		screen_hw_msg_pending = false;
	} else {
		SDL_RenderCopy(renderer, screen_texture,
			      screen_use_hw_scaling ? &screen_src_rect : NULL,
			      screen_use_hw_scaling ? &screen_dst_rect : NULL);
	}
	plat_sdl_profile_add(&sdl_video_profile.copy_us,
			     &sdl_video_profile.copy_max_us,
			     plat_get_ticks_us_u64() - start_us);

	if (sdl_flip_start_us) {
		uint64_t pre_present_us = plat_get_ticks_us_u64() - sdl_flip_start_us;

		plat_sdl_profile_add(&sdl_video_profile.pre_present_us,
				     &sdl_video_profile.pre_present_max_us,
				     pre_present_us);
		if (pre_present_us > (uint64_t)frame_time)
			sdl_video_profile.late_present_frames++;
	}

	start_us = plat_get_ticks_us_u64();
	SDL_RenderPresent(renderer);
	plat_sdl_profile_add(&sdl_video_profile.present_us,
			     &sdl_video_profile.present_max_us,
			     plat_get_ticks_us_u64() - start_us);
	plat_sdl_profile_frame();
	return screen_pixels;
#else
	SDL_Flip(screen);
	return screen->pixels;
#endif
}

void *plat_prepare_screenshot(int *w, int *h, int *bpp)
{
	if (w) *w = SCREEN_WIDTH;
	if (h) *h = SCREEN_HEIGHT;
	if (bpp) *bpp = SCREEN_BPP;

#ifdef USE_SDL2
	return screen_pixels;
#else
	return screen->pixels;
#endif
}

int plat_dump_screen(const char *filename) {
	char imgname[MAX_PATH];
	int ret = -1;
	SDL_Surface *surface = NULL;

	snprintf(imgname, MAX_PATH, "%s.bmp", filename);

	if (g_menuscreen_ptr) {
		surface = SDL_CreateRGBSurfaceFrom(g_menubg_src_ptr,
		                                   g_menubg_src_w,
		                                   g_menubg_src_h,
		                                   16,
		                                   g_menubg_src_w * sizeof(uint16_t),
		                                   0xF800, 0x07E0, 0x001F, 0x0000);
		if (surface) {
			ret = SDL_SaveBMP(surface, imgname);
			SDL_FreeSurface(surface);
		}
	} else {
#ifdef USE_SDL2
		if (screen_use_hw_scaling)
			plat_sdl_readback_screen();
#endif
		ret = SDL_SaveBMP(screen, imgname);
	}

	return ret;
}

int plat_load_screen(const char *filename, void *buf, size_t buf_size, int *w, int *h, int *bpp) {
	int ret = -1;
	char imgname[MAX_PATH];
	SDL_Surface *imgsurface = NULL;
	SDL_Surface *surface = NULL;

	snprintf(imgname, MAX_PATH, "%s.bmp", filename);
	imgsurface = SDL_LoadBMP(imgname);
	if (!imgsurface)
		goto finish;

#ifdef USE_SDL2
	surface = SDL_ConvertSurfaceFormat(imgsurface, SDL_PIXELFORMAT_RGB565, 0);
#else
	surface = SDL_DisplayFormat(imgsurface);
#endif
	if (!surface)
		goto finish;

	if (surface->pitch > SCREEN_PITCH ||
	    surface->h > SCREEN_HEIGHT ||
	    surface->w == 0 ||
	    surface->h * surface->pitch > buf_size)
		goto finish;

	memcpy(buf, surface->pixels, surface->pitch * surface->h);
	*w = surface->w;
	*h = surface->h;
	*bpp = surface->pitch / surface->w;

	ret = 0;

finish:
	if (imgsurface)
		SDL_FreeSurface(imgsurface);
	if (surface)
		SDL_FreeSurface(surface);
	return ret;
}


void plat_video_menu_enter(int is_rom_loaded)
{
	if (g_menuscreen_ptr)
		return;

#ifdef USE_SDL2
	if (screen_use_hw_scaling)
		plat_sdl_readback_screen();
	memcpy(g_menubg_src_ptr, screen_pixels, g_menubg_src_h * g_menubg_src_pp * sizeof(uint16_t));
#else
	SDL_LockSurface(screen);
	memcpy(g_menubg_src_ptr, screen->pixels, g_menubg_src_h * g_menubg_src_pp * sizeof(uint16_t));
	SDL_UnlockSurface(screen);
#endif
	g_menuscreen_ptr = fb_flip();
}

void plat_video_menu_begin(void)
{
#ifndef USE_SDL2
	SDL_LockSurface(screen);
#endif
	menu_begin();
}

void plat_video_menu_end(void)
{
	menu_end();
#ifndef USE_SDL2
	SDL_UnlockSurface(screen);
#endif
	g_menuscreen_ptr = fb_flip();
}

void plat_video_menu_leave(void)
{
	memset(g_menubg_src_ptr, 0, g_menuscreen_h * g_menuscreen_pp * sizeof(uint16_t));

#ifdef USE_SDL2
	memset(screen_pixels, 0, g_menuscreen_h * g_menuscreen_pp * sizeof(uint16_t));
	fb_flip();
	memset(screen_pixels, 0, g_menuscreen_h * g_menuscreen_pp * sizeof(uint16_t));
#else
	SDL_LockSurface(screen);
	memset(screen->pixels, 0, g_menuscreen_h * g_menuscreen_pp * sizeof(uint16_t));
	SDL_UnlockSurface(screen);
	fb_flip();
	SDL_LockSurface(screen);
	memset(screen->pixels, 0, g_menuscreen_h * g_menuscreen_pp * sizeof(uint16_t));
	SDL_UnlockSurface(screen);
#endif

	g_menuscreen_ptr = NULL;
}

void plat_video_open(void)
{
}

void plat_video_set_msg(const char *new_msg, unsigned priority, unsigned msec)
{
	if (!new_msg) {
		video_expire_msg();
	} else if (priority >= msg_priority) {
		snprintf(msg, HUD_LEN, "%s", new_msg);
		string_truncate(msg, HUD_LEN - 1);
		msg_priority = priority;
		msg_expire = plat_get_ticks_ms() + msec;
	}
}

void plat_video_process(const void *data, unsigned width, unsigned height, size_t pitch) {
	static int had_msg = 0;
	frame_dirty = true;
#ifdef USE_SDL2
	uint16_t *pixels = screen_pixels;
	unsigned screen_h = SCREEN_HEIGHT;
	unsigned screen_pitch = SCREEN_WIDTH;
	bool want_hw_scaling = plat_sdl_is_hw_scale_supported(width, height, pitch);
	bool want_linear = (scale_filter != SCALE_FILTER_NEAREST);
	Uint32 texture_format = plat_sdl_texture_format_for_pitch(width, pitch);
#else
	SDL_LockSurface(screen);
	uint16_t *pixels = screen->pixels;
	unsigned screen_h = screen->h;
	unsigned screen_pitch = screen->pitch / SCREEN_BPP;
#endif

	if (had_msg) {
		video_clear_msg(pixels, screen_h, screen_pitch);
		had_msg = 0;
	}

#ifdef USE_SDL2
	if (want_hw_scaling) {
		if (plat_sdl_ensure_screen_texture(width, height, pitch, texture_format, want_linear) == 0) {
			uint64_t start_us;

			plat_sdl_compute_hw_rects(width, height, &screen_src_rect, &screen_dst_rect);
			if (!screen_last_dst_rect_valid ||
			    memcmp(&screen_last_dst_rect, &screen_dst_rect, sizeof(screen_dst_rect)) != 0) {
				screen_last_dst_rect = screen_dst_rect;
				screen_last_dst_rect_valid = true;
				screen_clear_frames = 3;
			}
			start_us = plat_get_ticks_us_u64();
			SDL_UpdateTexture(screen_texture, NULL, data, (int)pitch);
			plat_sdl_profile_add(&sdl_video_profile.update_us,
					     &sdl_video_profile.update_max_us,
					     plat_get_ticks_us_u64() - start_us);
			screen_use_hw_scaling = true;
			if (screen_last_log_w != width ||
			    screen_last_log_h != height ||
			    screen_last_log_pitch != pitch) {
				PA_INFO("SDL HW video: %ux%u pitch=%zu format=%s src=%dx%d+%d+%d dst=%dx%d+%d+%d\n",
					width, height, pitch,
					texture_format == SDL_PIXELFORMAT_XRGB8888 ? "XRGB8888" : "RGB565",
					screen_src_rect.w, screen_src_rect.h, screen_src_rect.x, screen_src_rect.y,
					screen_dst_rect.w, screen_dst_rect.h, screen_dst_rect.x, screen_dst_rect.y);
				screen_last_log_w = width;
				screen_last_log_h = height;
				screen_last_log_pitch = pitch;
			}
		} else {
			screen_use_hw_scaling = false;
			screen_last_dst_rect_valid = false;
			scale(width, height, pitch, data, pixels);
		}
	} else {
		if (plat_sdl_ensure_screen_texture(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_PITCH, SDL_PIXELFORMAT_RGB565, false) == 0)
			screen_use_hw_scaling = false;
		screen_last_dst_rect_valid = false;
		scale(width, height, pitch, data, pixels);
	}
#else
	scale(width, height, pitch, data, pixels);
#endif

	if (msg[0]) {
		if (screen_use_hw_scaling) {
			if (plat_sdl_ensure_screen_copy_texture() == 0) {
				uint64_t start_us = plat_get_ticks_us_u64();
				SDL_RenderClear(renderer);
				SDL_RenderCopy(renderer, screen_texture, &screen_src_rect, &screen_dst_rect);
				plat_sdl_readback_screen();
				video_print_msg(pixels, screen_h, screen_pitch, msg);
				SDL_UpdateTexture(screen_copy_texture, NULL, screen_pixels, SCREEN_PITCH);
				plat_sdl_profile_add(&sdl_video_profile.hud_us,
						     &sdl_video_profile.hud_max_us,
						     plat_get_ticks_us_u64() - start_us);
				screen_hw_msg_pending = true;
				had_msg = 1;
				video_update_msg();
				return;
			}
		}
		video_print_msg(pixels, screen_h, screen_pitch, msg);
		had_msg = 1;
	}

#ifndef USE_SDL2
	SDL_UnlockSurface(screen);
#endif

	video_update_msg();
}

void plat_video_dupe(void)
{
	frame_dirty = true;
}

void plat_video_flip(void)
{
	static uint64_t next_frame_time_us = 0;

	if (frame_dirty) {
#ifdef USE_SDL2
		uint64_t frame_audio_wait_us = sdl_video_profile.audio_wait_frame_us;

		if (frame_audio_wait_us > sdl_video_profile.audio_wait_frame_max_us)
			sdl_video_profile.audio_wait_frame_max_us =
				(frame_audio_wait_us > UINT32_MAX) ? UINT32_MAX : (uint32_t)frame_audio_wait_us;
		sdl_video_profile.audio_wait_frame_us = 0;
		sdl_flip_start_us = plat_get_ticks_us_u64();
#endif
		if (enable_drc && !screen_renderer_vsync) {
			uint64_t time = plat_get_ticks_us_u64();

			if (limit_frames && time < next_frame_time_us) {
				uint32_t delaytime = (next_frame_time_us - time - 1) / 1000 + 1;
#ifdef USE_SDL2
				uint64_t pace_start_us = plat_get_ticks_us_u64();
#endif

				if (delaytime < 1000)
					SDL_Delay(delaytime);
				else
					next_frame_time_us = 0;
#ifdef USE_SDL2
				plat_sdl_profile_add(&sdl_video_profile.pace_us,
						     &sdl_video_profile.pace_max_us,
						     plat_get_ticks_us_u64() - pace_start_us);
#endif

				time = plat_get_ticks_us_u64();
			}

			if (!next_frame_time_us || !limit_frames) {
				next_frame_time_us = time;
			}

			fb_flip();

			do {
				next_frame_time_us += frame_time;
			} while (next_frame_time_us < time);
		} else {
			fb_flip();
			next_frame_time_us = 0;
		}

		frame_dirty = false;
	}
}

void plat_video_close(void)
{
}

unsigned plat_cpu_ticks(void)
{
	long unsigned ticks = 0;
	long ticksps = 0;
	FILE *file = NULL;

	file = fopen("/proc/self/stat", "r");
	if (!file)
		goto finish;

	if (!fscanf(file, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu", &ticks))
		goto finish;

	ticksps = sysconf(_SC_CLK_TCK);

	if (ticksps)
		ticks = ticks * 100 / ticksps;

finish:
	if (file)
		fclose(file);

	return ticks;
}

static void plat_sound_callback(void *unused, uint8_t *stream, int len)
{
	int16_t *p = (int16_t *)stream;
	if (audio.buf_len == 0) {
		memset(stream, 0, len);
		return;
	}

	len /= (sizeof(int16_t) * 2);

	while (audio.buf_r != audio.buf_w && len > 0) {
		*p++ = audio.buf[audio.buf_r].left;
		*p++ = audio.buf[audio.buf_r].right;
		audio.max_buf_w = audio.buf_r;

		len--;
		audio.buf_r++;

		if (audio.buf_r >= audio.buf_len) audio.buf_r = 0;
	}

	while(len > 0) {
		*p++ = 0;
		*p++ = 0;
		--len;
	}
}

static void plat_sound_finish(void)
{
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	if (audio.buf) {
		free(audio.buf);
		audio.buf = NULL;
	}
}

static int plat_sound_init(void)
{
	int requested_sample_rate = sample_rate > 0 ? sample_rate : 44100;

	if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		return -1;
	}

	SDL_AudioSpec spec, received;

	spec.freq = MIN(requested_sample_rate, MAX_SAMPLE_RATE);
	spec.format = AUDIO_S16;
	spec.channels = 1;
	spec.samples = 4096;
	spec.callback = plat_sound_callback;

	if (SDL_OpenAudio(&spec, &received) < 0) {
		plat_sound_finish();
		return -1;
	}

	audio.in_sample_rate = requested_sample_rate;
	audio.out_sample_rate = received.freq;
	audio.sample_rate_adj = audio.out_sample_rate * DRC_MAX_ADJUSTMENT;
	audio.adj_out_sample_rate = audio.out_sample_rate;

	plat_sound_select_resampler();
	plat_sound_resize_buffer();

	SDL_PauseAudio(0);
	return 0;
}

int plat_sound_occupancy(void)
{
	int buffered = 0;
	if (audio.buf_len == 0)
		return 0;

	if (audio.buf_w != audio.buf_r) {
		buffered = audio.buf_w > audio.buf_r ?
			audio.buf_w - audio.buf_r :
			(audio.buf_w + audio.buf_len) - audio.buf_r;
	}

	return buffered * 100 / audio.buf_len;
}

#define BATCH_SIZE 100
void plat_sound_write_resample(const struct audio_frame *data, int frames, int (*resample)(struct audio_frame data), bool drc)
{
	int consumed = 0;
	if (audio.buf_len == 0)
		return;

	if (drc) {
		int occupancy = plat_sound_occupancy();

		if (occupancy < DRC_ADJ_BELOW) {
			audio.adj_out_sample_rate = audio.out_sample_rate + audio.sample_rate_adj;
		} else if (occupancy > DRC_ADJ_ABOVE) {
			audio.adj_out_sample_rate = audio.out_sample_rate - audio.sample_rate_adj;
		} else {
			audio.adj_out_sample_rate = audio.out_sample_rate;
		}
	}

	SDL_LockAudio();

	while (frames > 0) {
		int tries = 0;
		int amount = MIN(BATCH_SIZE, frames);

		while (tries < 10 && audio.buf_w == audio.max_buf_w) {
#ifdef USE_SDL2
			uint64_t wait_start_us;
#endif

			tries++;
			SDL_UnlockAudio();

			if (!limit_frames)
				return;

#ifdef USE_SDL2
			wait_start_us = plat_get_ticks_us_u64();
#endif
			plat_sleep_ms(1);
#ifdef USE_SDL2
			{
				uint64_t wait_us = plat_get_ticks_us_u64() - wait_start_us;
				plat_sdl_profile_add(&sdl_video_profile.audio_wait_us,
						     &sdl_video_profile.audio_wait_max_us,
						     wait_us);
				sdl_video_profile.audio_wait_frame_us += wait_us;
			}
#endif
			SDL_LockAudio();
		}

		while (amount && audio.buf_w != audio.max_buf_w) {
			consumed = resample(*data);
			data += consumed;
			amount -= consumed;
			frames -= consumed;
		}
	}
	SDL_UnlockAudio();
}

void plat_sound_write_passthrough(const struct audio_frame *data, int frames)
{
	plat_sound_write_resample(data, frames, audio_resample_passthrough, false);
}

void plat_sound_write_nearest(const struct audio_frame *data, int frames)
{
	plat_sound_write_resample(data, frames, audio_resample_nearest, false);
}

void plat_sound_write_drc(const struct audio_frame *data, int frames)
{
	plat_sound_write_resample(data, frames, audio_resample_nearest, true);
}

void plat_sound_resize_buffer(void) {
	size_t buf_size;
	SDL_LockAudio();

	audio.buf_len = frame_rate > 0
		? current_audio_buffer_size * audio.in_sample_rate / frame_rate
		: 0;

		/* Dynamic adjustment keeps buffer 50% full, need double size */
	if (enable_drc)
		audio.buf_len *= 2;

	if (audio.buf_len == 0) {
		SDL_UnlockAudio();
		return;
	}

	buf_size = audio.buf_len * sizeof(struct audio_frame);
	audio.buf = realloc(audio.buf, buf_size);

	if (!audio.buf) {
		SDL_UnlockAudio();
		PA_ERROR("Error initializing sound buffer\n");
		plat_sound_finish();
		return;
	}

	memset(audio.buf, 0, buf_size);
	audio.buf_w = 0;
	audio.buf_r = 0;
	audio.max_buf_w = audio.buf_len - 1;
	SDL_UnlockAudio();
}

static void plat_sound_select_resampler(void)
{
	if (enable_drc) {
		PA_INFO("Using audio adjustment (in: %d, out: %d-%d)\n", audio.in_sample_rate, audio.out_sample_rate - audio.sample_rate_adj, audio.out_sample_rate + audio.sample_rate_adj);
		plat_sound_write = plat_sound_write_drc;
	} else if (audio.in_sample_rate == audio.out_sample_rate) {
		PA_INFO("Using passthrough resampler (in: %d, out: %d)\n", audio.in_sample_rate, audio.out_sample_rate);
		plat_sound_write = plat_sound_write_passthrough;
	} else {
		PA_INFO("Using nearest resampler (in: %d, out: %d)\n", audio.in_sample_rate, audio.out_sample_rate);
		plat_sound_write = plat_sound_write_nearest;
	}
}

void plat_sdl_event_handler(void *event_)
{
}

int plat_init(void)
{
	plat_sound_write = plat_sound_write_nearest;

#ifdef USE_SDL2
	plat_sdl_set_platform_defaults();

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		PA_ERROR("%s, failed to init SDL video: %s\n", __func__, SDL_GetError());
		plat_sdl_log_video_drivers();
		return -1;
	}

	window = SDL_CreateWindow("picoarch",
	                          SDL_WINDOWPOS_CENTERED,
	                          SDL_WINDOWPOS_CENTERED,
	                          SCREEN_WIDTH,
	                          SCREEN_HEIGHT,
#ifdef SF3000
	                          SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
#else
	                          SDL_WINDOW_SHOWN);
#endif
	if (!window) {
		PA_ERROR("%s, failed to create window: %s\n", __func__, SDL_GetError());
		return -1;
	}

	screen_renderer_vsync = false;
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
	renderer = SDL_CreateRenderer(window, -1,
	                              SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		PA_WARN("%s, accelerated renderer unavailable: %s\n", __func__, SDL_GetError());
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	}
	if (!renderer) {
		PA_ERROR("%s, failed to create accelerated renderer: %s\n", __func__, SDL_GetError());
		return -1;
	}
	{
		SDL_RendererInfo info;
		if (SDL_GetRendererInfo(renderer, &info) == 0) {
			PA_INFO("SDL video driver: %s\n",
				SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown");
			PA_INFO("SDL renderer: %s%s%s%s\n",
				info.name ? info.name : "unknown",
				(info.flags & SDL_RENDERER_ACCELERATED) ? " accelerated" : "",
				(info.flags & SDL_RENDERER_SOFTWARE) ? " software" : "",
				(info.flags & SDL_RENDERER_PRESENTVSYNC) ? " vsync" : "");
			screen_renderer_vsync = !!(info.flags & SDL_RENDERER_PRESENTVSYNC);
#ifdef SF3000
			if (screen_renderer_vsync && !getenv("PICOARCH_TRUST_RENDERER_VSYNC")) {
				PA_INFO("SF3000: ignoring renderer vsync flag; using manual frame pacing\n");
				screen_renderer_vsync = false;
			}
#endif
			if (screen_renderer_vsync)
				PA_INFO("SDL renderer vsync active; skipping manual frame delay\n");
		}
	}

	screen_texture = NULL;
	screen_texture_ready = false;
	screen_texture_linear = false;
	screen_use_hw_scaling = false;
	screen_hw_msg_pending = false;
	screen_clear_frames = 3;
	screen_last_dst_rect_valid = false;
	screen_tex_format = 0;
	screen_tex_w = 0;
	screen_tex_h = 0;
	screen_tex_pitch = 0;
	screen_last_log_w = 0;
	screen_last_log_h = 0;
	screen_last_log_pitch = 0;
	screen_copy_texture = NULL;

	screen = SDL_CreateRGBSurfaceFrom(screen_pixels,
	                                  SCREEN_WIDTH,
	                                  SCREEN_HEIGHT,
	                                  SCREEN_BPP * 8,
	                                  SCREEN_PITCH,
	                                  0xF800, 0x07E0, 0x001F, 0x0000);
	if (!screen) {
		PA_ERROR("%s, failed to create screen surface: %s\n", __func__, SDL_GetError());
		return -1;
	}
#else
	SDL_Init(SDL_INIT_VIDEO);
	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP * 8, SDL_SWSURFACE);
	if (screen == NULL) {
		PA_ERROR("%s, failed to set video mode\n", __func__);
		return -1;
	}
#endif

	SDL_ShowCursor(0);

	g_menuscreen_w = SCREEN_WIDTH;
	g_menuscreen_h = SCREEN_HEIGHT;
	g_menuscreen_pp = SCREEN_WIDTH;
	g_menuscreen_ptr = NULL;

	g_menubg_src_w = SCREEN_WIDTH;
	g_menubg_src_h = SCREEN_HEIGHT;
	g_menubg_src_pp = SCREEN_WIDTH;

	if (in_sdl_init(&in_sdl_platform_data, plat_sdl_event_handler)) {
		PA_ERROR("SDL input failed to init: %s\n", SDL_GetError());
		return -1;
	}
	in_probe();

	if (plat_sound_init()) {
		PA_ERROR("SDL sound failed to init: %s\n", SDL_GetError());
		return -1;
	}
	return 0;
}

int plat_reinit(void)
{
	if (sample_rate && sample_rate != audio.in_sample_rate) {
		plat_sound_finish();

		if (plat_sound_init()) {
			PA_ERROR("SDL sound failed to init: %s\n", SDL_GetError());
			return -1;
		}
	} else {
		plat_sound_resize_buffer();
		plat_sound_select_resampler();
	}

	if (frame_rate != 0)
		frame_time = 1000000 / frame_rate;

	scale_update_scaler();
	return 0;
}

void plat_finish(void)
{
	plat_sound_finish();
#ifdef USE_SDL2
	if (screen)
		SDL_FreeSurface(screen);
	plat_sdl_destroy_screen_texture();
	plat_sdl_destroy_screen_copy_texture();
	if (renderer)
		SDL_DestroyRenderer(renderer);
	if (window)
		SDL_DestroyWindow(window);
	screen = NULL;
	screen_texture = NULL;
	renderer = NULL;
	window = NULL;
#else
	SDL_FreeSurface(screen);
	screen = NULL;
#endif
	SDL_Quit();
}
