#ifndef BARE_GFX_H
#define BARE_GFX_H

/* Zero-Dependency, Bare-Metal Graphics Layer */

/* Virtual Coordinate System (The "SVG" approach)
	0 to 10000 represents 0.00% to 100.00% of the screen.
*/
#define BGFX_VMAX 10000

/* Event System */
typedef enum {
	BGFX_EVT_NONE = 0,
	BGFX_EVT_MOUSE_MOVE,
	BGFX_EVT_MOUSE_BTN,
	BGFX_EVT_KEY
} bgfx_event_type;

typedef struct {
	bgfx_event_type type;
	unsigned int data1; /* e.g., virtual X or Keycode */
	unsigned int data2; /* e.g., virtual Y */
	unsigned int state; /* 1 for press, 0 for release */
} bgfx_event;

/* Window / Surface Structure for Compositing */
typedef struct {
	unsigned int x, y;         /* Virtual Position */
	unsigned int w, h;         /* Virtual Dimensions */
	unsigned int* buffer;      /* The actual pixel data for this window */
	unsigned int physical_w;   /* Physical width of the raw buffer */
	unsigned int physical_h;   /* Physical height of the raw buffer */
	unsigned int z_index;      /* Higher = closer to user */
	unsigned char is_active;
	unsigned char is_hidden;
} bgfx_window;

#define BGFX_EVENT_QUEUE_SIZE 64

/* Graphics Context */
typedef struct {
	unsigned int* front_buffer; /* Provided by UEFI GOP / Multiboot */
	unsigned int* back_buffer;  /* Kernel allocated off-screen buffer */
	unsigned int width;         /* Physical width */
	unsigned int height;        /* Physical height */
	unsigned int pitch;         /* Physical pitch (pixels per scanline) */

	/* Damage Tracking (Dirty Rectangle) */
	unsigned int dirty_min_x, dirty_min_y;
	unsigned int dirty_max_x, dirty_max_y;
	unsigned char is_dirty;

	/* Lock-free ring buffer for IRQ handlers */
	bgfx_event evt_queue[BGFX_EVENT_QUEUE_SIZE];
	volatile unsigned int evt_head;
	volatile unsigned int evt_tail;
} bgfx_context;

/* API Declarations */
void bgfx_init(bgfx_context* ctx, unsigned int* fb, unsigned int* bb, unsigned int w, unsigned int h, unsigned int p);
int  bgfx_push_event(bgfx_context* ctx, bgfx_event evt);
int  bgfx_poll_event(bgfx_context* ctx, bgfx_event* evt);
void bgfx_draw_rect_v(bgfx_context* ctx, unsigned int vx, unsigned int vy, unsigned int vw, unsigned int vh, unsigned int color);

/* NEW: Blits a pixel buffer onto the screen using virtual scaling and optional 50% alpha blending */
void bgfx_blit_surface_v(bgfx_context* ctx, unsigned int* src, unsigned int src_w, unsigned int src_h, unsigned int vx, unsigned int vy, unsigned int vw, unsigned int vh, unsigned char apply_alpha);

void bgfx_swap_buffers(bgfx_context* ctx);

#endif /* BARE_GFX_H */

#ifdef BARE_GFX_IMPLEMENTATION

/* Translates Virtual Coords (0-10000) to Physical Screen Pixels */
static inline unsigned int bgfx_v2p_x(bgfx_context* ctx, unsigned int vx) {
	return (vx * ctx->width) / BGFX_VMAX;
}

static inline unsigned int bgfx_v2p_y(bgfx_context* ctx, unsigned int vy) {
	return (vy * ctx->height) / BGFX_VMAX;
}

/* Updates the damage rectangle to encompass new drawings */
static void bgfx_mark_dirty(bgfx_context* ctx, unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2) {
	if (!ctx->is_dirty) {
		ctx->dirty_min_x = x1;
		ctx->dirty_min_y = y1;
		ctx->dirty_max_x = x2;
		ctx->dirty_max_y = y2;
		ctx->is_dirty = 1;
	} else {
		if (x1 < ctx->dirty_min_x) ctx->dirty_min_x = x1;
		if (y1 < ctx->dirty_min_y) ctx->dirty_min_y = y1;
		if (x2 > ctx->dirty_max_x) ctx->dirty_max_x = x2;
		if (y2 > ctx->dirty_max_y) ctx->dirty_max_y = y2;
	}
}

void bgfx_init(bgfx_context* ctx, unsigned int* fb, unsigned int* bb, unsigned int w, unsigned int h, unsigned int p) {
	ctx->front_buffer = fb;
	ctx->back_buffer = bb;
	ctx->width = w;
	ctx->height = h;
	ctx->pitch = p;
	ctx->is_dirty = 0;
	ctx->evt_head = 0;
	ctx->evt_tail = 0;
}

int bgfx_push_event(bgfx_context* ctx, bgfx_event evt) {
	unsigned int next = (ctx->evt_head + 1) % BGFX_EVENT_QUEUE_SIZE;
	if (next == ctx->evt_tail) return 0; /* Queue full, drop event */
	
	ctx->evt_queue[ctx->evt_head] = evt;
	ctx->evt_head = next;
	return 1;
}

int bgfx_poll_event(bgfx_context* ctx, bgfx_event* evt) {
	if (ctx->evt_head == ctx->evt_tail) return 0; /* Queue empty */
	
	*evt = ctx->evt_queue[ctx->evt_tail];
	ctx->evt_tail = (ctx->evt_tail + 1) % BGFX_EVENT_QUEUE_SIZE;
	return 1;
}

void bgfx_draw_rect_v(bgfx_context* ctx, unsigned int vx, unsigned int vy, unsigned int vw, unsigned int vh, unsigned int color) {
	unsigned int px = bgfx_v2p_x(ctx, vx);
	unsigned int py = bgfx_v2p_y(ctx, vy);
	unsigned int pw = bgfx_v2p_x(ctx, vw);
	unsigned int ph = bgfx_v2p_y(ctx, vh);

	/* Bounds checking */
	if (px >= ctx->width || py >= ctx->height) return;
	if (px + pw > ctx->width) pw = ctx->width - px;
	if (py + ph > ctx->height) ph = ctx->height - py;

	/* Draw to back buffer */
	for (unsigned int y = 0; y < ph; y++) {
		unsigned int* row = ctx->back_buffer + ((py + y) * ctx->pitch) + px;
		for (unsigned int x = 0; x < pw; x++) {
			row[x] = color;
		}
	}

	bgfx_mark_dirty(ctx, px, py, px + pw, py + ph);
}

void bgfx_blit_surface_v(bgfx_context* ctx, unsigned int* src, unsigned int src_w, unsigned int src_h, unsigned int vx, unsigned int vy, unsigned int vw, unsigned int vh, unsigned char apply_alpha) {
	unsigned int px = bgfx_v2p_x(ctx, vx);
	unsigned int py = bgfx_v2p_y(ctx, vy);
	unsigned int pw = bgfx_v2p_x(ctx, vw);
	unsigned int ph = bgfx_v2p_y(ctx, vh);

	/* Bounds checking */
	if (px >= ctx->width || py >= ctx->height) return;
	if (px + pw > ctx->width) pw = ctx->width - px;
	if (py + ph > ctx->height) ph = ctx->height - py;

	for (unsigned int y = 0; y < ph; y++) {
		unsigned int* dst_row = ctx->back_buffer + ((py + y) * ctx->pitch) + px;
		unsigned int sy = (y * src_h) / ph; /* Nearest neighbor Y scaling */
		unsigned int* src_row = src + (sy * src_w);

		for (unsigned int x = 0; x < pw; x++) {
			unsigned int sx = (x * src_w) / pw; /* Nearest neighbor X scaling */
			unsigned int src_pixel = src_row[sx];

			if (apply_alpha) {
				unsigned int dst_pixel = dst_row[x];
				/* Fast 50% blend: masks out 8-bit bounds to prevent channel bleed during shift */
				dst_row[x] = ((src_pixel & 0x00FEFEFE) >> 1) + ((dst_pixel & 0x00FEFEFE) >> 1);
			} else {
				dst_row[x] = src_pixel;
			}
		}
	}

	bgfx_mark_dirty(ctx, px, py, px + pw, py + ph);
}

void bgfx_swap_buffers(bgfx_context* ctx) {
	if (!ctx->is_dirty) return; /* Nothing changed */

	unsigned int x1 = ctx->dirty_min_x;
	unsigned int y1 = ctx->dirty_min_y;
	unsigned int x2 = ctx->dirty_max_x;
	unsigned int y2 = ctx->dirty_max_y;

	for (unsigned int y = y1; y < y2; y++) {
		unsigned int offset = (y * ctx->pitch) + x1;
		unsigned int* src = ctx->back_buffer + offset;
		unsigned int* dst = ctx->front_buffer + offset;
		unsigned int width = x2 - x1;
		
		for (unsigned int i = 0; i < width; i++) {
			dst[i] = src[i];
		}
	}

	ctx->is_dirty = 0;
}

#endif /* BARE_GFX_IMPLEMENTATION */