/****************************************************************************
 * app/launcher/launcher_main.c
 *
 * Contest 2026 team 349 (FedoraVForce) LVGL launcher demo
 * VSAI LogicPi A1 (Amlogic S6): MIPI DSI 1080x1920 + GT9XX touch
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/param.h>

#include <malloc.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LAUNCHER_SCREEN_W     1080
#define LAUNCHER_SCREEN_H     1920

#define LAUNCHER_BG_COLOR     0x0e1821
#define LAUNCHER_CARD_COLOR   0x17262f
#define LAUNCHER_ACCENT       0x00a8f3
#define LAUNCHER_TEXT_DIM     0x8fb0bf

#define UNLOCK_DRAG_DIST      240
#define PAINT_BRUSH_RADIUS    6
#define PAINT_STROKE_STEP     4
#define CANVAS_BUF_SIZE       (LAUNCHER_SCREEN_W * LAUNCHER_SCREEN_H * \
                               sizeof(uint16_t))
#define TOUCH_DEVPATH         "/dev/input0"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static lv_obj_t *g_lock_scr;
static lv_obj_t *g_home_scr;
static lv_obj_t *g_about_scr;
static lv_obj_t *g_paint_scr;

static lv_obj_t *g_lock_clock;
static lv_obj_t *g_home_clock;
static lv_obj_t *g_home_uptime;
static lv_obj_t *g_about_heap;
static lv_obj_t *g_about_fps;
static lv_obj_t *g_home_fps;

static lv_obj_t *g_canvas;
static uint16_t *g_canvas_buf;
static uint16_t g_brush_color;
static lv_point_t g_brush_prev;
static lv_point_t g_drag_start;

static const uint32_t g_palette[] =
{
  0x000000, 0xe8443a, 0x00a8f3, 0x27c24a
};

static uint32_t g_refr_count;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t launcher_uptime_sec(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)ts.tv_sec;
}

static void launcher_format_clock(char *buf, size_t buflen, uint32_t sec)
{
  snprintf(buf, buflen, "%02u:%02u:%02u",
           (sec / 3600) % 100, (sec / 60) % 60, sec % 60);
}

static lv_obj_t *launcher_label(lv_obj_t *parent, const char *text,
                                const lv_font_t *font, uint32_t color,
                                lv_align_t align, int32_t x, int32_t y)
{
  lv_obj_t *label = lv_label_create(parent);

  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_label_set_text(label, text);
  lv_obj_align(label, align, x, y);
  return label;
}

static void launcher_switch_scr(lv_obj_t *scr, lv_screen_load_anim_t anim)
{
  lv_screen_load_anim(scr, anim, 250, 0, false);
}

static void launcher_refr_event_cb(lv_event_t *e)
{
  UNUSED(e);
  g_refr_count++;
}

/****************************************************************************
 * Name: lock screen
 ****************************************************************************/

static void launcher_unlock(void)
{
  launcher_switch_scr(g_home_scr, LV_SCR_LOAD_ANIM_MOVE_TOP);
}

static void launcher_lock_swipe_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *indev = lv_indev_active();
  lv_point_t p;

  if (indev == NULL)
    {
      return;
    }

  lv_indev_get_point(indev, &p);

  if (code == LV_EVENT_PRESSED)
    {
      g_drag_start = p;
    }
  else if (code == LV_EVENT_PRESSING)
    {
      if (g_drag_start.y - p.y > UNLOCK_DRAG_DIST)
        {
          lv_indev_wait_release(indev);
          launcher_unlock();
        }
    }
}

static void launcher_unlock_event_cb(lv_event_t *e)
{
  UNUSED(e);
  launcher_unlock();
}

static void launcher_create_lock_scr(void)
{
  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_t *obj;

  lv_obj_set_style_bg_color(scr, lv_color_hex(LAUNCHER_BG_COLOR), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_add_event_cb(scr, launcher_lock_swipe_event_cb, LV_EVENT_PRESSED,
                      NULL);
  lv_obj_add_event_cb(scr, launcher_lock_swipe_event_cb, LV_EVENT_PRESSING,
                      NULL);

  g_lock_clock = launcher_label(scr, "00:00:00", &lv_font_montserrat_48,
                                0xffffff, LV_ALIGN_CENTER, 0, -260);
  launcher_label(scr, "openvela on LogicPi A1", &lv_font_montserrat_24,
                 LAUNCHER_TEXT_DIM, LV_ALIGN_CENTER, 0, -160);

  obj = lv_obj_create(scr);
  lv_obj_set_size(obj, 180, 180);
  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(obj, lv_color_hex(LAUNCHER_ACCENT), 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_align(obj, LV_ALIGN_BOTTOM_MID, 0, -300);
  lv_obj_add_event_cb(obj, launcher_unlock_event_cb, LV_EVENT_SHORT_CLICKED,
                      NULL);

  launcher_label(obj, LV_SYMBOL_UP, &lv_font_montserrat_48,
                 LAUNCHER_BG_COLOR, LV_ALIGN_CENTER, 0, 0);
  launcher_label(scr, "Swipe up or tap to unlock", &lv_font_montserrat_16,
                 LAUNCHER_TEXT_DIM, LV_ALIGN_BOTTOM_MID, 0, -100);

  g_lock_scr = scr;
}

/****************************************************************************
 * Name: about screen
 ****************************************************************************/

static void launcher_back_event_cb(lv_event_t *e)
{
  UNUSED(e);
  launcher_switch_scr(g_home_scr, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
}

static void launcher_about_info_line(lv_obj_t *parent, const char *key,
                                     const char *val, int32_t y)
{
  launcher_label(parent, key, &lv_font_montserrat_16, LAUNCHER_TEXT_DIM,
                 LV_ALIGN_TOP_LEFT, 60, y);
  launcher_label(parent, val, &lv_font_montserrat_24, 0xffffff,
                 LV_ALIGN_TOP_LEFT, 60, y + 30);
}

static void launcher_create_about_scr(void)
{
  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_t *obj;

  lv_obj_set_style_bg_color(scr, lv_color_hex(LAUNCHER_BG_COLOR), 0);

  launcher_label(scr, "About this port", &lv_font_montserrat_48, 0xffffff,
                 LV_ALIGN_TOP_MID, 0, 100);

  launcher_about_info_line(scr, "BOARD", "VSAI LogicPi A1", 240);
  launcher_about_info_line(scr, "SOC", "Amlogic S6 (ARM64)", 320);
  launcher_about_info_line(scr, "INTERRUPT", "GICv3", 400);
  launcher_about_info_line(scr, "DISPLAY", "MIPI DSI 1080x1920 RGB565",
                           480);
  launcher_about_info_line(scr, "TOUCH", "GT9XX I2C", 560);

  g_about_heap = launcher_label(scr, "heap: -", &lv_font_montserrat_24,
                                0xffffff, LV_ALIGN_TOP_LEFT, 60, 660);
  g_about_fps = launcher_label(scr, "fps: -", &lv_font_montserrat_24,
                               0xffffff, LV_ALIGN_TOP_LEFT, 60, 720);

  obj = lv_button_create(scr);
  lv_obj_set_size(obj, 440, 140);
  lv_obj_set_style_radius(obj, 40, 0);
  lv_obj_align(obj, LV_ALIGN_BOTTOM_MID, 0, -120);
  lv_obj_add_event_cb(obj, launcher_back_event_cb, LV_EVENT_CLICKED, NULL);

  launcher_label(obj, LV_SYMBOL_LEFT " Back", &lv_font_montserrat_24,
                 0xffffff, LV_ALIGN_CENTER, 0, 0);

  g_about_scr = scr;
}

/****************************************************************************
 * Name: paint screen
 ****************************************************************************/

static void launcher_paint_dot(int32_t x, int32_t y)
{
  int32_t dx;
  int32_t dy;

  for (dy = -PAINT_BRUSH_RADIUS; dy <= PAINT_BRUSH_RADIUS; dy++)
    {
      for (dx = -PAINT_BRUSH_RADIUS; dx <= PAINT_BRUSH_RADIUS; dx++)
        {
          if (dx * dx + dy * dy <=
              PAINT_BRUSH_RADIUS * PAINT_BRUSH_RADIUS &&
              x + dx >= 0 && x + dx < LAUNCHER_SCREEN_W &&
              y + dy >= 0 && y + dy < LAUNCHER_SCREEN_H)
            {
              g_canvas_buf[(y + dy) * LAUNCHER_SCREEN_W + x + dx] =
                g_brush_color;
            }
        }
    }
}

static void launcher_paint_stroke(int32_t x0, int32_t y0,
                                  int32_t x1, int32_t y1)
{
  int32_t dx = x1 - x0;
  int32_t dy = y1 - y0;
  int32_t steps = LV_MAX(LV_ABS(dx), LV_ABS(dy));
  int32_t i;

  if (steps == 0)
    {
      launcher_paint_dot(x1, y1);
      return;
    }

  for (i = 0; i <= steps; i += PAINT_STROKE_STEP)
    {
      launcher_paint_dot(x0 + dx * i / steps, y0 + dy * i / steps);
    }
}

static void launcher_paint_invalidate(int32_t x0, int32_t y0,
                                      int32_t x1, int32_t y1)
{
  lv_area_t area =
    {
      LV_MIN(x0, x1) - PAINT_BRUSH_RADIUS - 2,
      LV_MIN(y0, y1) - PAINT_BRUSH_RADIUS - 2,
      LV_MAX(x0, x1) + PAINT_BRUSH_RADIUS + 2,
      LV_MAX(y0, y1) + PAINT_BRUSH_RADIUS + 2
    };

  lv_obj_invalidate_area(g_canvas, &area);
}

static void launcher_canvas_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *indev = lv_indev_active();
  lv_point_t p;

  if (indev == NULL)
    {
      return;
    }

  lv_indev_get_point(indev, &p);

  if (code == LV_EVENT_PRESSED)
    {
      g_brush_prev = p;
    }

  launcher_paint_stroke(g_brush_prev.x, g_brush_prev.y, p.x, p.y);
  launcher_paint_invalidate(g_brush_prev.x, g_brush_prev.y, p.x, p.y);
  g_brush_prev = p;
}

static void launcher_paint_clear_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_canvas_fill_bg(g_canvas, lv_color_hex(0xffffff), LV_OPA_COVER);
}

static void launcher_paint_color_cb(lv_event_t *e)
{
  const lv_color_t *color = lv_event_get_user_data(e);

  g_brush_color = lv_color_to_u16(*color);
}

static void launcher_paint_color_btn(lv_obj_t *parent, int idx)
{
  static lv_color_t button_colors[nitems(g_palette)];
  lv_obj_t *btn = lv_button_create(parent);
  lv_color_t c = lv_color_hex(g_palette[idx]);

  button_colors[idx] = c;
  lv_obj_set_size(btn, 70, 70);
  lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(btn, c, 0);
  lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 280 + idx * 90, 25);
  lv_obj_add_event_cb(btn, launcher_paint_color_cb, LV_EVENT_CLICKED,
                      &button_colors[idx]);
}

static bool launcher_create_paint_scr(void)
{
  lv_obj_t *scr;
  lv_obj_t *obj;
  int i;

  g_canvas_buf = memalign(64, CANVAS_BUF_SIZE);
  if (g_canvas_buf == NULL)
    {
      fprintf(stderr, "launcher: canvas alloc failed\n");
      return false;
    }

  scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0xffffff), 0);

  g_canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(g_canvas, g_canvas_buf, LAUNCHER_SCREEN_W,
                       LAUNCHER_SCREEN_H, LV_COLOR_FORMAT_RGB565);
  lv_canvas_fill_bg(g_canvas, lv_color_hex(0xffffff), LV_OPA_COVER);
  lv_obj_add_flag(g_canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_canvas, launcher_canvas_event_cb, LV_EVENT_PRESSED,
                      NULL);
  lv_obj_add_event_cb(g_canvas, launcher_canvas_event_cb, LV_EVENT_PRESSING,
                      NULL);

  obj = lv_obj_create(scr);
  lv_obj_set_size(obj, LAUNCHER_SCREEN_W, 120);
  lv_obj_set_style_bg_color(obj, lv_color_hex(LAUNCHER_CARD_COLOR), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_90, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, 0);

  obj = lv_button_create(scr);
  lv_obj_set_size(obj, 200, 70);
  lv_obj_set_style_radius(obj, 24, 0);
  lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 40, 25);
  lv_obj_add_event_cb(obj, launcher_back_event_cb, LV_EVENT_CLICKED, NULL);

  launcher_label(obj, LV_SYMBOL_LEFT " Back", &lv_font_montserrat_16,
                 0xffffff, LV_ALIGN_CENTER, 0, 0);

  obj = lv_button_create(scr);
  lv_obj_set_size(obj, 200, 70);
  lv_obj_set_style_radius(obj, 24, 0);
  lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -40, 25);
  lv_obj_add_event_cb(obj, launcher_paint_clear_cb, LV_EVENT_CLICKED,
                      NULL);

  launcher_label(obj, LV_SYMBOL_TRASH " Clear", &lv_font_montserrat_16,
                 0xffffff, LV_ALIGN_CENTER, 0, 0);

  for (i = 0; i < nitems(g_palette); i++)
    {
      launcher_paint_color_btn(scr, i);
    }

  g_brush_color = lv_color_to_u16(lv_color_hex(g_palette[0]));
  g_paint_scr = scr;
  return true;
}

/****************************************************************************
 * Name: home screen
 ****************************************************************************/

static void launcher_home_btn_event_cb(lv_event_t *e)
{
  lv_obj_t *target = lv_event_get_user_data(e);

  launcher_switch_scr(target, LV_SCR_LOAD_ANIM_MOVE_TOP);
}

static void launcher_home_btn(lv_obj_t *parent, const char *icon,
                              const char *text, lv_obj_t *target,
                              int32_t x)
{
  lv_obj_t *btn = lv_button_create(parent);

  lv_obj_set_size(btn, 440, 300);
  lv_obj_set_style_radius(btn, 48, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(LAUNCHER_CARD_COLOR), 0);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, x, -180);
  lv_obj_add_event_cb(btn, launcher_home_btn_event_cb, LV_EVENT_CLICKED,
                      target);

  launcher_label(btn, icon, &lv_font_montserrat_48, LAUNCHER_ACCENT,
                 LV_ALIGN_TOP_MID, 0, 50);
  launcher_label(btn, text, &lv_font_montserrat_24, 0xffffff,
                 LV_ALIGN_BOTTOM_MID, 0, -50);
}

static void launcher_create_home_scr(void)
{
  lv_obj_t *scr = lv_obj_create(NULL);

  lv_obj_set_style_bg_color(scr, lv_color_hex(LAUNCHER_BG_COLOR), 0);

  launcher_label(scr, "FedoraVForce", &lv_font_montserrat_16,
                 LAUNCHER_TEXT_DIM, LV_ALIGN_TOP_LEFT, 60, 60);
  g_home_fps = launcher_label(scr, "- fps", &lv_font_montserrat_16,
                              LAUNCHER_TEXT_DIM, LV_ALIGN_TOP_RIGHT,
                              -60, 60);
  g_home_clock = launcher_label(scr, "00:00:00", &lv_font_montserrat_48,
                                0xffffff, LV_ALIGN_CENTER, 0, -320);
  g_home_uptime = launcher_label(scr, "up -", &lv_font_montserrat_24,
                                 LAUNCHER_TEXT_DIM, LV_ALIGN_CENTER, 0,
                                 -240);
  launcher_label(scr, LV_SYMBOL_SETTINGS " NuttX / openvela",
                 &lv_font_montserrat_24, LAUNCHER_ACCENT, LV_ALIGN_CENTER,
                 0, 200);

  launcher_home_btn(scr, LV_SYMBOL_LIST, "About", g_about_scr, -240);
  launcher_home_btn(scr, LV_SYMBOL_EDIT, "Paint", g_paint_scr, 240);

  g_home_scr = scr;
}

static void launcher_periodic_cb(lv_timer_t *timer)
{
  char buf[32];
  struct mallinfo mi = mallinfo();
  uint32_t sec = launcher_uptime_sec();
  uint32_t fps = g_refr_count;

  UNUSED(timer);

  g_refr_count = 0;
  launcher_format_clock(buf, sizeof(buf), sec);

  if (lv_screen_active() == g_lock_scr)
    {
      lv_label_set_text(g_lock_clock, buf);
    }
  else if (lv_screen_active() == g_home_scr)
    {
      lv_label_set_text(g_home_clock, buf);
      lv_label_set_text_fmt(g_home_uptime, "up %us - heap %uKB free",
                            sec, mi.fordblks / 1024);
    }

  lv_label_set_text_fmt(g_about_fps, "fps: %u", fps);
  lv_label_set_text_fmt(g_about_heap, "heap: %uKB free", mi.fordblks / 1024);
  lv_label_set_text_fmt(g_home_fps, "%u fps", fps);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;

  UNUSED(argc);
  UNUSED(argv);

  lv_init();

  lv_nuttx_dsc_init(&info);
  info.input_path = TOUCH_DEVPATH;
  lv_nuttx_init(&info, &result);

  if (result.disp == NULL)
    {
      fprintf(stderr, "launcher: no display found\n");
      return 1;
    }

  if (result.indev == NULL)
    {
      fprintf(stderr, "launcher: no touchscreen found\n");
      return 1;
    }

  lv_display_add_event_cb(result.disp, launcher_refr_event_cb,
                          LV_EVENT_REFR_START, NULL);

  launcher_create_about_scr();
  if (!launcher_create_paint_scr())
    {
      return 1;
    }

  launcher_create_home_scr();
  launcher_create_lock_scr();

  lv_screen_load(g_lock_scr);
  launcher_periodic_cb(NULL);
  lv_timer_create(launcher_periodic_cb, 1000, NULL);

  while (1)
    {
      uint32_t idle = lv_timer_handler();

      idle = idle ? idle : 1;
      usleep(idle * 1000);
    }

  return 0;
}
