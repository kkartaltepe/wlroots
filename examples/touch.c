#define _POSIX_C_SOURCE 199309L
#define _XOPEN_SOURCE 500
#include <GLES2/gl2.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_list.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include "support/cat.h"
#include "support/shared.h"

struct sample_state {
	struct wlr_renderer *renderer;
	struct wlr_texture *cat_texture;
	struct wl_list touch_points;
};

struct touch_point {
	int32_t touch_id;
	double x, y;
	struct wl_list link;
};

static void handle_output_frame(struct output_state *output, struct timespec *ts) {
	struct compositor_state *state = output->compositor;
	struct sample_state *sample = state->data;
	struct wlr_output *wlr_output = output->output;

	int32_t width, height;
	wlr_output_effective_resolution(wlr_output, &width, &height);

	wlr_output_make_current(wlr_output, NULL);
	wlr_renderer_begin(sample->renderer, wlr_output->width, wlr_output->height);
	wlr_renderer_clear(sample->renderer, (float[]){0.25f, 0.25f, 0.25f, 1});

	int tex_width, tex_height;
	wlr_texture_get_size(sample->cat_texture, &tex_width, &tex_height);

	struct touch_point *p;
	wl_list_for_each(p, &sample->touch_points, link) {
		int x = (int)(p->x * width) - tex_width / 2;
		int y = (int)(p->y * height) - tex_height / 2;
		wlr_render_texture(sample->renderer, sample->cat_texture,
			wlr_output->transform_matrix, x, y, 1.0f);
	}

	wlr_renderer_end(sample->renderer);
	wlr_output_swap_buffers(wlr_output, NULL, NULL);
}

static void handle_touch_down(struct touch_state *tstate,
		int32_t touch_id, double x, double y) {
	struct sample_state *sample = tstate->compositor->data;
	struct touch_point *point = calloc(1, sizeof(struct touch_point));
	point->touch_id = touch_id;
	point->x = x;
	point->y = y;
	wl_list_insert(&sample->touch_points, &point->link);
}

static void handle_touch_up(struct touch_state *tstate, int32_t touch_id) {
	struct sample_state *sample = tstate->compositor->data;
	struct touch_point *point, *tmp;
	wl_list_for_each_safe(point, tmp, &sample->touch_points, link) {
		if (point->touch_id == touch_id) {
			wl_list_remove(&point->link);
			break;
		}
	}
}

static void handle_touch_motion(struct touch_state *tstate,
		int32_t touch_id, double x, double y) {
	struct sample_state *sample = tstate->compositor->data;
	struct touch_point *point;
	wl_list_for_each(point, &sample->touch_points, link) {
		if (point->touch_id == touch_id) {
			point->x = x;
			point->y = y;
			break;
		}
	}
}

int main(int argc, char *argv[]) {
	wlr_log_init(L_DEBUG, NULL);
	struct sample_state state;
	wl_list_init(&state.touch_points);

	struct compositor_state compositor = { 0,
		.data = &state,
		.output_frame_cb = handle_output_frame,
		.touch_down_cb = handle_touch_down,
		.touch_up_cb = handle_touch_up,
		.touch_motion_cb = handle_touch_motion,
	};
	compositor_init(&compositor);

	state.renderer = wlr_backend_get_renderer(compositor.backend);
	if (!state.renderer) {
		wlr_log(L_ERROR, "Could not start compositor, OOM");
		exit(EXIT_FAILURE);
	}
	state.cat_texture = wlr_texture_from_pixels(state.renderer,
		WL_SHM_FORMAT_ARGB8888, cat_tex.width * 4, cat_tex.width, cat_tex.height,
		cat_tex.pixel_data);
	if (!state.cat_texture) {
		wlr_log(L_ERROR, "Could not start compositor, OOM");
		exit(EXIT_FAILURE);
	}

	if (!wlr_backend_start(compositor.backend)) {
		wlr_log(L_ERROR, "Failed to start backend");
		wlr_backend_destroy(compositor.backend);
		exit(1);
	}
	wl_display_run(compositor.display);

	wlr_texture_destroy(state.cat_texture);
	wlr_renderer_destroy(state.renderer);
	compositor_fini(&compositor);
}
