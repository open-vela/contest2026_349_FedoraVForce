/****************************************************************************
 * app/paint/paint_main.c
 *
 * LVGL paint app
 * VSAI LogicPi A1 (Amlogic S6): MIPI DSI 1080x1920 + GT9XX touch
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/param.h>

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PAINT_SCREEN_W        1080
#define PAINT_SCREEN_H        1920
#define PAINT_BUF_SIZE        (PAINT_SCREEN_W * PAINT_SCREEN_H * \
                               sizeof(uint16_t))

#define PAINT_BAR_COLOR       0x1c2b42
#define PAINT_BTN_COLOR       0x2a3d5a
#define PAINT_BTN_PRESSED     0x3b5378
#define PAINT_BTN_SIZE        96
#define PAINT_SWATCH_SIZE     64

/****************************************************************************
 * Private Data
 ****************************************************************************/

static lv_obj_t *g_canvas;
static lv_obj_t *g_swatch;
static lv_obj_t *g_size_dot;
static uint16_t *g_canvas_buf;
static uint16_t g_brush_color;
static lv_point_t g_brush_prev;
static int g_brush_size;
static bool g_running;

static const uint32_t g_palette[] =
{
  0x111111, 0xffffff, 0xe8443a, 0xf5a623, 0x27c24a, 0x00a8f3, 0x8e5cf7
};

static const int32_t g_brush_radius[] =
{
  4, 10, 20
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static lv_obj_t *paint_button(lv_obj_t *parent, int32_t size,
                              uint32_t color, lv_event_cb_t cb)
{
  lv_obj_t *btn = lv_obj_create(parent);

  lv_obj_remove_style_all(btn);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(btn, size, size);
  lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
  return btn;
}

static void paint_icon_button(lv_obj_t *parent, const char *symbol,
                              lv_event_cb_t cb)
{
  lv_obj_t *btn = paint_button(parent, PAINT_BTN_SIZE, PAINT_BTN_COLOR, cb);
  lv_obj_t *label = lv_label_create(btn);

  lv_obj_set_style_bg_color(btn, lv_color_hex(PAINT_BTN_PRESSED),
                            LV_STATE_PRESSED);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_label_set_text(label, symbol);
  lv_obj_center(label);
}

static void paint_dot(int32_t x, int32_t y)
{
  int32_t r = g_brush_radius[g_brush_size];
  int32_t dx;
  int32_t dy;

  for (dy = -r; dy <= r; dy++)
    {
      for (dx = -r; dx <= r; dx++)
        {
          if (dx * dx + dy * dy <= r * r &&
              x + dx >= 0 && x + dx < PAINT_SCREEN_W &&
              y + dy >= 0 && y + dy < PAINT_SCREEN_H)
            {
              g_canvas_buf[(y + dy) * PAINT_SCREEN_W + x + dx] =
                g_brush_color;
            }
        }
    }
}

static void paint_stroke(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
  int32_t r = g_brush_radius[g_brush_size];
  int32_t dx = x1 - x0;
  int32_t dy = y1 - y0;
  int32_t steps = MAX(LV_ABS(dx), LV_ABS(dy));
  int32_t i;
  lv_area_t area =
    {
      MIN(x0, x1) - r, MIN(y0, y1) - r, MAX(x0, x1) + r, MAX(y0, y1) + r
    };

  for (i = 0; i < steps; i += r / 2 + 1)
    {
      paint_dot(x0 + dx * i / steps, y0 + dy * i / steps);
    }

  paint_dot(x1, y1);
  lv_obj_invalidate_area(g_canvas, &area);
}

static void paint_canvas_event_cb(lv_event_t *e)
{
  lv_point_t p;

  lv_indev_get_point(lv_indev_active(), &p);

  if (lv_event_get_code(e) == LV_EVENT_PRESSED)
    {
      g_brush_prev = p;
    }

  paint_stroke(g_brush_prev.x, g_brush_prev.y, p.x, p.y);
  g_brush_prev = p;
}

static void paint_home_cb(lv_event_t *e)
{
  UNUSED(e);
  g_running = false;
}

static void paint_clear_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_canvas_fill_bg(g_canvas, lv_color_white(), LV_OPA_COVER);
}

static void paint_size_cb(lv_event_t *e)
{
  int32_t d;

  UNUSED(e);

  g_brush_size = (g_brush_size + 1) % nitems(g_brush_radius);
  d = 2 * g_brush_radius[g_brush_size];
  lv_obj_set_size(g_size_dot, d, d);
}

static void paint_color_cb(lv_event_t *e)
{
  lv_obj_t *swatch = lv_event_get_target(e);
  lv_color_t c = lv_obj_get_style_bg_color(swatch, 0);

  lv_obj_remove_state(g_swatch, LV_STATE_CHECKED);
  lv_obj_add_state(swatch, LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(g_size_dot, c, 0);
  g_brush_color = lv_color_to_u16(c);
  g_swatch = swatch;
}

static void paint_create(void)
{
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *bar;
  lv_obj_t *obj;
  int i;

  g_canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(g_canvas, g_canvas_buf, PAINT_SCREEN_W,
                       PAINT_SCREEN_H, LV_COLOR_FORMAT_RGB565);
  lv_canvas_fill_bg(g_canvas, lv_color_white(), LV_OPA_COVER);
  lv_obj_add_flag(g_canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_canvas, paint_canvas_event_cb, LV_EVENT_PRESSED,
                      NULL);
  lv_obj_add_event_cb(g_canvas, paint_canvas_event_cb, LV_EVENT_PRESSING,
                      NULL);

  /* Floating tool bar: home, palette, brush size, clear */

  bar = lv_obj_create(scr);
  lv_obj_remove_style_all(bar);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(bar, PAINT_SCREEN_W - 48, 136);
  lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_90, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(PAINT_BAR_COLOR), 0);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 24);

  paint_icon_button(bar, LV_SYMBOL_HOME, paint_home_cb);

  for (i = 0; i < nitems(g_palette); i++)
    {
      obj = paint_button(bar, PAINT_SWATCH_SIZE, g_palette[i],
                         paint_color_cb);
      lv_obj_set_style_outline_color(obj, lv_color_white(),
                                     LV_STATE_CHECKED);
      lv_obj_set_style_outline_width(obj, 5, LV_STATE_CHECKED);
      lv_obj_set_style_outline_pad(obj, 5, LV_STATE_CHECKED);
      if (i == 0)
        {
          g_swatch = obj;
        }
    }

  obj = paint_button(bar, PAINT_BTN_SIZE, PAINT_BTN_COLOR, paint_size_cb);
  lv_obj_set_style_bg_color(obj, lv_color_hex(PAINT_BTN_PRESSED),
                            LV_STATE_PRESSED);

  g_size_dot = lv_obj_create(obj);
  lv_obj_remove_style_all(g_size_dot);
  lv_obj_remove_flag(g_size_dot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(g_size_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(g_size_dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(g_size_dot, lv_color_white(), 0);
  lv_obj_set_style_border_width(g_size_dot, 2, 0);
  lv_obj_center(g_size_dot);

  paint_icon_button(bar, LV_SYMBOL_TRASH, paint_clear_cb);

  /* Start with the medium brush and the first color */

  g_brush_size = 0;
  paint_size_cb(NULL);
  lv_obj_send_event(g_swatch, LV_EVENT_CLICKED, NULL);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;
  uint32_t idle;

  UNUSED(argc);
  UNUSED(argv);

  /* LVGL is one global instance, shared by every task of the flat build */

  if (lv_is_initialized())
    {
      fprintf(stderr, "paint: LVGL is busy, start it from the launcher\n");
      return 1;
    }

  g_canvas_buf = memalign(64, PAINT_BUF_SIZE);
  if (g_canvas_buf == NULL)
    {
      fprintf(stderr, "paint: canvas alloc failed\n");
      return 1;
    }

  lv_init();

  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, &result);

  if (result.disp == NULL || result.indev == NULL)
    {
      fprintf(stderr, "paint: no display or touchscreen\n");
      g_running = false;
    }
  else
    {
      paint_create();
      g_running = true;
    }

  while (g_running)
    {
      idle = lv_timer_handler();
      usleep(MAX(idle, 1) * 1000);
    }

  lv_nuttx_deinit(&result);
  lv_deinit();
  free(g_canvas_buf);
  return 0;
}
