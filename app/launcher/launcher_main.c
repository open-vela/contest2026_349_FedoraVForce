/****************************************************************************
 * app/launcher/launcher_main.c
 *
 * LVGL launcher
 * VSAI LogicPi A1 (Amlogic S6): MIPI DSI 1080x1920 + GT9XX touch
 *
 * Apps are NSH builtins listed in CONFIG_LVX_LAUNCHER_APPS.
 * LVGL is a single global instance, so the launcher releases it before
 * starting an app and initializes it again after the app exits.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/param.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/lib/builtin.h>

#include <lvgl/lvgl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LAUNCHER_SCREEN_W     1080
#define LAUNCHER_SCREEN_H     1920

#define LAUNCHER_BG_COLOR     0x15181f
#define LAUNCHER_LOCK_COLOR   0x2b2f8f
#define LAUNCHER_CARD_COLOR   0x1f232c
#define LAUNCHER_LIVE_COLOR   0x3b40d4
#define LAUNCHER_SYSTEM_COLOR 0x4a5568
#define LAUNCHER_SLEEP_COLOR  0xb4235a
#define LAUNCHER_ACCENT       0x5b9cff
#define LAUNCHER_TEXT_DIM     0x9aa3b5

/* Tiles sit on a 4 column grid of 238px cells with 16px gaps */

#define LAUNCHER_MARGIN       40
#define LAUNCHER_TILE_GAP     16
#define LAUNCHER_TILE_STEP    254
#define LAUNCHER_TILE_RADIUS  14
#define LAUNCHER_TILES_Y      200
#define LAUNCHER_LIVE_H       (2 * LAUNCHER_TILE_STEP - LAUNCHER_TILE_GAP)

#define LAUNCHER_ANIM_TIME    300
#define LAUNCHER_ZOOM_MAX     1024
#define LAUNCHER_FLIP_PERIOD  4000
#define UNLOCK_DRAG_DIST      300

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void launcher_lock_event_cb(lv_event_t *e);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static lv_nuttx_result_t g_result;

static lv_obj_t *g_lock;
static lv_obj_t *g_home_scr;
static lv_obj_t *g_home_tiles;
static lv_obj_t *g_about_scr;

static lv_obj_t *g_live_pane;
static lv_obj_t *g_lock_clock;
static lv_obj_t *g_home_clock;
static lv_obj_t *g_home_heap;
static lv_obj_t *g_about_uptime;
static lv_obj_t *g_about_heap;

static lv_obj_t *g_return_tile;
static lv_area_t g_zoom_area;
static int32_t g_drag_start;
static int g_launch;

static const lv_style_prop_t g_press_props[] =
{
  LV_STYLE_TRANSFORM_WIDTH, LV_STYLE_TRANSFORM_HEIGHT, 0
};

static lv_style_transition_dsc_t g_press_trans;

static const uint32_t g_app_colors[] =
{
  0x1f6feb, 0xe5484d, 0x2ea043, 0xe8a33d, 0x8250df, 0x0e9aa7
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

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

static lv_obj_t *launcher_dim(lv_obj_t *label)
{
  lv_obj_set_style_text_opa(label, LV_OPA_70, 0);
  return label;
}

static lv_obj_t *launcher_box(lv_obj_t *parent, uint32_t color,
                              int32_t radius)
{
  lv_obj_t *obj = lv_obj_create(parent);

  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(obj, radius, 0);
  return obj;
}

/* Outlined round button, used for back and unlock */

static lv_obj_t *launcher_ring(lv_obj_t *parent, const char *symbol,
                               lv_event_cb_t cb)
{
  lv_obj_t *obj = launcher_box(parent, 0xffffff, LV_RADIUS_CIRCLE);

  lv_obj_set_size(obj, 104, 104);
  lv_obj_set_style_bg_opa(obj, LV_OPA_10, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_40, LV_STATE_PRESSED);
  lv_obj_set_style_border_color(obj, lv_color_white(), 0);
  lv_obj_set_style_border_width(obj, 3, 0);
  lv_obj_add_event_cb(obj, cb, LV_EVENT_SHORT_CLICKED, NULL);
  launcher_label(obj, symbol, &lv_font_montserrat_24, 0xffffff,
                 LV_ALIGN_CENTER, 0, 0);
  return obj;
}

static lv_obj_t *launcher_screen(void)
{
  lv_obj_t *scr = lv_obj_create(NULL);

  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr, lv_color_hex(LAUNCHER_BG_COLOR), 0);
  lv_obj_set_style_bg_grad_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
  return scr;
}

/* Montserrat stops at 48px, so big text is drawn at twice that size */

static void launcher_zoom_label(lv_obj_t *label, int32_t pivot_x,
                                int32_t pivot_y)
{
  lv_obj_set_style_transform_scale(label, 2 * LV_SCALE_NONE, 0);
  lv_obj_set_style_transform_pivot_x(label, lv_pct(pivot_x), 0);
  lv_obj_set_style_transform_pivot_y(label, lv_pct(pivot_y), 0);
}

static void launcher_anim(void *var, lv_anim_exec_xcb_t exec_cb,
                          int32_t from, int32_t to, uint32_t delay,
                          lv_anim_path_cb_t path_cb,
                          lv_anim_completed_cb_t completed_cb)
{
  lv_anim_t a;

  lv_anim_init(&a);
  lv_anim_set_var(&a, var);
  lv_anim_set_exec_cb(&a, exec_cb);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, LAUNCHER_ANIM_TIME);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_path_cb(&a, path_cb);
  lv_anim_set_completed_cb(&a, completed_cb);
  lv_anim_start(&a);
}

static void launcher_translate_x_cb(void *obj, int32_t v)
{
  lv_obj_set_style_translate_x(obj, v, 0);
}

static void launcher_translate_y_cb(void *obj, int32_t v)
{
  lv_obj_set_style_translate_y(obj, v, 0);
}

static lv_obj_t *launcher_info_row(lv_obj_t *parent, const char *key,
                                   const char *val)
{
  lv_obj_t *row = lv_obj_create(parent);

  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  launcher_label(row, key, &lv_font_montserrat_24, LAUNCHER_TEXT_DIM,
                 LV_ALIGN_DEFAULT, 0, 0);
  return launcher_label(row, val, &lv_font_montserrat_24, 0xffffff,
                        LV_ALIGN_DEFAULT, 0, 0);
}

static lv_obj_t *launcher_card(lv_obj_t *parent, const char *title)
{
  lv_obj_t *card = launcher_box(parent, LAUNCHER_CARD_COLOR,
                                LAUNCHER_TILE_RADIUS);

  lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(card, 44, 0);
  lv_obj_set_style_pad_row(card, 32, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

  launcher_label(card, title, &lv_font_montserrat_16, LAUNCHER_ACCENT,
                 LV_ALIGN_DEFAULT, 0, 0);
  return card;
}

/****************************************************************************
 * Name: tiles
 ****************************************************************************/

static lv_obj_t *launcher_tile(lv_obj_t *parent, uint32_t color,
                               const char *glyph, const char *name)
{
  lv_obj_t *tile = launcher_box(parent, color, LAUNCHER_TILE_RADIUS);

  lv_obj_set_style_bg_grad_color(tile,
                                 lv_color_darken(lv_color_hex(color),
                                                 LV_OPA_30), 0);
  lv_obj_set_style_bg_grad_dir(tile, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_transform_width(tile, -8, LV_STATE_PRESSED);
  lv_obj_set_style_transform_height(tile, -8, LV_STATE_PRESSED);
  lv_obj_set_style_transition(tile, &g_press_trans, 0);

  if (glyph != NULL)
    {
      launcher_label(tile, glyph, &lv_font_montserrat_48, 0xffffff,
                     LV_ALIGN_CENTER, 0, -16);
    }

  if (name != NULL)
    {
      launcher_label(tile, name, &lv_font_montserrat_24, 0xffffff,
                     LV_ALIGN_BOTTOM_LEFT, 24, -20);
    }

  return tile;
}

/* Position and size are in grid cells */

static void launcher_place(lv_obj_t *tile, int col, int row, int w, int h)
{
  lv_obj_set_pos(tile, col * LAUNCHER_TILE_STEP, row * LAUNCHER_TILE_STEP);
  lv_obj_set_size(tile, w * LAUNCHER_TILE_STEP - LAUNCHER_TILE_GAP,
                  h * LAUNCHER_TILE_STEP - LAUNCHER_TILE_GAP);
}

static lv_obj_t *launcher_app_tile(lv_obj_t *parent, int index,
                                   bool named)
{
  FAR const char *name = builtin_getname(index);
  char title[32];
  char letter[2];
  lv_obj_t *tile;

  snprintf(title, sizeof(title), "%c%s", toupper(name[0]), name + 1);
  letter[0] = title[0];
  letter[1] = '\0';

  tile = launcher_tile(parent, g_app_colors[index % nitems(g_app_colors)],
                       letter, named ? title : NULL);
  launcher_zoom_label(lv_obj_get_child(tile, 0), 50, 50);
  return tile;
}

/* Grow or shrink a top layer tile between g_zoom_area and full screen */

static void launcher_zoom_cb(void *obj, int32_t v)
{
  lv_area_t *a = &g_zoom_area;

  lv_obj_set_pos(obj, lv_map(v, 0, LAUNCHER_ZOOM_MAX, a->x1, 0),
                 lv_map(v, 0, LAUNCHER_ZOOM_MAX, a->y1, 0));
  lv_obj_set_size(obj,
                  lv_map(v, 0, LAUNCHER_ZOOM_MAX, lv_area_get_width(a),
                         LAUNCHER_SCREEN_W),
                  lv_map(v, 0, LAUNCHER_ZOOM_MAX, lv_area_get_height(a),
                         LAUNCHER_SCREEN_H));
  lv_obj_set_style_radius(obj, lv_map(v, 0, LAUNCHER_ZOOM_MAX,
                                      LAUNCHER_TILE_RADIUS, 0), 0);
}

static void launcher_zoomed_cb(lv_anim_t *a)
{
  g_launch = (int)(intptr_t)lv_obj_get_user_data(a->var);
}

static void launcher_app_event_cb(lv_event_t *e)
{
  int index = (int)(intptr_t)lv_event_get_user_data(e);
  lv_obj_t *obj = launcher_app_tile(lv_layer_top(), index, false);

  /* Block further taps until the app is started */

  lv_obj_add_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_user_data(obj, (void *)(intptr_t)index);
  lv_obj_get_coords(lv_event_get_target(e), &g_zoom_area);
  launcher_anim(obj, launcher_zoom_cb, 0, LAUNCHER_ZOOM_MAX, 0,
                lv_anim_path_ease_in_out, launcher_zoomed_cb);
}

static void launcher_about_open_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_screen_load_anim(g_about_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT,
                      LAUNCHER_ANIM_TIME, 0, false);
}

static void launcher_flip_cb(lv_timer_t *timer)
{
  int32_t y = lv_obj_get_style_translate_y(g_live_pane, 0);

  UNUSED(timer);
  launcher_anim(g_live_pane, launcher_translate_y_cb, y,
                y < 0 ? 0 : -LAUNCHER_LIVE_H, 0, lv_anim_path_ease_in_out,
                NULL);
}

/* Live tile: uptime and free heap faces stacked in a pane that flips */

static void launcher_create_live_tile(lv_obj_t *parent)
{
  lv_obj_t *tile = launcher_tile(parent, LAUNCHER_LIVE_COLOR, NULL, NULL);
  lv_obj_t *pane;
  lv_obj_t *label;

  launcher_place(tile, 0, 0, 4, 2);
  lv_obj_add_event_cb(tile, launcher_about_open_cb, LV_EVENT_CLICKED,
                      NULL);

  pane = lv_obj_create(tile);
  lv_obj_remove_style_all(pane);
  lv_obj_remove_flag(pane, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(pane, lv_pct(100), 2 * LAUNCHER_LIVE_H);

  launcher_dim(launcher_label(pane, "UPTIME", &lv_font_montserrat_16,
                              0xffffff, LV_ALIGN_TOP_LEFT, 44, 40));
  g_home_clock = launcher_label(pane, "00:00:00", &lv_font_montserrat_48,
                                0xffffff, LV_ALIGN_TOP_LEFT, 44, 110);
  launcher_zoom_label(g_home_clock, 0, 0);
  launcher_dim(launcher_label(pane, "openvela on LogicPi A1",
                              &lv_font_montserrat_24, 0xffffff,
                              LV_ALIGN_TOP_LEFT, 44,
                              LAUNCHER_LIVE_H - 80));

  launcher_dim(launcher_label(pane, "FREE HEAP", &lv_font_montserrat_16,
                              0xffffff, LV_ALIGN_TOP_LEFT, 44,
                              LAUNCHER_LIVE_H + 40));
  g_home_heap = launcher_label(pane, "-", &lv_font_montserrat_48,
                               0xffffff, LV_ALIGN_TOP_LEFT, 44,
                               LAUNCHER_LIVE_H + 110);
  launcher_zoom_label(g_home_heap, 0, 0);
  label = launcher_dim(launcher_label(pane, "", &lv_font_montserrat_24,
                                      0xffffff, LV_ALIGN_TOP_LEFT, 44,
                                      2 * LAUNCHER_LIVE_H - 80));
  lv_label_set_text_fmt(label, "NuttX + LVGL %d.%d", LVGL_VERSION_MAJOR,
                        LVGL_VERSION_MINOR);

  g_live_pane = pane;
}

/* Layout: live tile, then a 2x2 tile per app, then the small tiles in
 * the next free 2x2 block.
 */

static void launcher_create_tiles(lv_obj_t *parent, int last)
{
  char list[] = CONFIG_LVX_LAUNCHER_APPS;
  FAR char *save;
  FAR char *name;
  lv_obj_t *tile;
  int block = 0;
  int index;

  launcher_create_live_tile(parent);

  for (name = strtok_r(list, " ", &save); name != NULL;
       name = strtok_r(NULL, " ", &save))
    {
      index = builtin_isavail(name);
      if (index < 0)
        {
          continue;
        }

      tile = launcher_app_tile(parent, index, true);
      launcher_place(tile, block % 2 * 2, 2 + block / 2 * 2, 2, 2);
      lv_obj_add_event_cb(tile, launcher_app_event_cb, LV_EVENT_CLICKED,
                          (void *)(intptr_t)index);
      if (index == last)
        {
          g_return_tile = tile;
        }

      block++;
    }

  tile = launcher_tile(parent, LAUNCHER_SYSTEM_COLOR, LV_SYMBOL_SETTINGS,
                       "System");
  launcher_place(tile, block % 2 * 2, 2 + block / 2 * 2, 1, 1);
  lv_obj_add_event_cb(tile, launcher_about_open_cb, LV_EVENT_CLICKED,
                      NULL);

  tile = launcher_tile(parent, LAUNCHER_SLEEP_COLOR, LV_SYMBOL_POWER,
                       "Lock");
  launcher_place(tile, block % 2 * 2 + 1, 2 + block / 2 * 2, 1, 1);
  lv_obj_add_event_cb(tile, launcher_lock_event_cb, LV_EVENT_CLICKED, NULL);
}

/****************************************************************************
 * Name: about screen
 ****************************************************************************/

static void launcher_about_close_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_screen_load_anim(g_home_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                      LAUNCHER_ANIM_TIME, 0, false);
}

static void launcher_create_about_scr(void)
{
  lv_obj_t *scr = launcher_screen();
  lv_obj_t *body;
  lv_obj_t *card;
  lv_obj_t *obj;

  obj = launcher_ring(scr, LV_SYMBOL_LEFT, launcher_about_close_cb);
  lv_obj_align(obj, LV_ALIGN_TOP_LEFT, LAUNCHER_MARGIN, 60);
  launcher_label(scr, "System", &lv_font_montserrat_48, 0xffffff,
                 LV_ALIGN_TOP_LEFT, LAUNCHER_MARGIN + 132, 82);

  body = lv_obj_create(scr);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, LAUNCHER_SCREEN_W - 2 * LAUNCHER_MARGIN,
                  LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(body, LAUNCHER_TILE_GAP, 0);
  lv_obj_align(body, LV_ALIGN_TOP_MID, 0, LAUNCHER_TILES_Y);

  card = launcher_card(body, "HARDWARE");
  launcher_info_row(card, "Board", "VSAI LogicPi A1");
  launcher_info_row(card, "SoC", "Amlogic S6 (ARM64)");
  launcher_info_row(card, "Interrupt", "GICv3");
  launcher_info_row(card, "Display", "MIPI DSI 1080x1920");
  launcher_info_row(card, "Touch", "GT9XX I2C");

  card = launcher_card(body, "SOFTWARE");
  launcher_info_row(card, "OS", "NuttX / openvela");
  lv_label_set_text_fmt(launcher_info_row(card, "Graphics", ""),
                        "LVGL %d.%d", LVGL_VERSION_MAJOR,
                        LVGL_VERSION_MINOR);
  g_about_uptime = launcher_info_row(card, "Uptime", "-");
  g_about_heap = launcher_info_row(card, "Free heap", "-");

  g_about_scr = scr;
}

/****************************************************************************
 * Name: home screen
 ****************************************************************************/

static void launcher_create_home_scr(int last)
{
  lv_obj_t *scr = launcher_screen();

  launcher_label(scr, "openvela", &lv_font_montserrat_48, 0xffffff,
                 LV_ALIGN_TOP_LEFT, LAUNCHER_MARGIN, 72);

  launcher_label(scr, "LogicPi A1", &lv_font_montserrat_24,
                 LAUNCHER_TEXT_DIM, LV_ALIGN_TOP_RIGHT, -LAUNCHER_MARGIN,
                 92);

  g_home_tiles = lv_obj_create(scr);
  lv_obj_remove_style_all(g_home_tiles);
  lv_obj_set_scroll_dir(g_home_tiles, LV_DIR_VER);
  lv_obj_set_style_pad_bottom(g_home_tiles, LAUNCHER_MARGIN, 0);
  lv_obj_set_size(g_home_tiles, LAUNCHER_SCREEN_W - 2 * LAUNCHER_MARGIN,
                  LAUNCHER_SCREEN_H - LAUNCHER_TILES_Y);
  lv_obj_set_pos(g_home_tiles, LAUNCHER_MARGIN, LAUNCHER_TILES_Y);
  launcher_create_tiles(g_home_tiles, last);

  g_home_scr = scr;
}

/* Tiles fly in from the right, top left first */

static void launcher_home_intro(void)
{
  lv_obj_t *tile;
  uint32_t i;

  for (i = 0; i < lv_obj_get_child_count(g_home_tiles); i++)
    {
      tile = lv_obj_get_child(g_home_tiles, i);
      launcher_anim(tile, launcher_translate_x_cb, LAUNCHER_SCREEN_W, 0,
                    (lv_obj_get_style_x(tile, 0) +
                     lv_obj_get_style_y(tile, 0)) /
                    LAUNCHER_TILE_STEP * 50,
                    lv_anim_path_ease_out, NULL);
    }
}

/****************************************************************************
 * Name: lock curtain
 ****************************************************************************/

static void launcher_unlocked_cb(lv_anim_t *a)
{
  lv_obj_add_flag(a->var, LV_OBJ_FLAG_HIDDEN);
}

static void launcher_unlock(void)
{
  lv_obj_remove_flag(g_home_tiles, LV_OBJ_FLAG_HIDDEN);
  launcher_home_intro();
  launcher_anim(g_lock, launcher_translate_y_cb,
                lv_obj_get_style_translate_y(g_lock, 0), -LAUNCHER_SCREEN_H,
                0, lv_anim_path_ease_in, launcher_unlocked_cb);
}

static void launcher_unlock_event_cb(lv_event_t *e)
{
  UNUSED(e);
  launcher_unlock();
}

/* Tiles stay hidden under the curtain and fly in when it is lifted */

static void launcher_locked_cb(lv_anim_t *a)
{
  UNUSED(a);
  lv_obj_add_flag(g_home_tiles, LV_OBJ_FLAG_HIDDEN);
}

static void launcher_lock_event_cb(lv_event_t *e)
{
  UNUSED(e);
  lv_obj_remove_flag(g_lock, LV_OBJ_FLAG_HIDDEN);
  launcher_anim(g_lock, launcher_translate_y_cb, -LAUNCHER_SCREEN_H, 0, 0,
                lv_anim_path_ease_out, launcher_locked_cb);
}

static void launcher_drag_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_point_t p;
  int32_t dy;

  lv_indev_get_point(lv_indev_active(), &p);
  dy = MAX(g_drag_start - p.y, 0);

  if (code == LV_EVENT_PRESSED)
    {
      g_drag_start = p.y;
    }
  else if (code == LV_EVENT_PRESSING)
    {
      lv_obj_set_style_translate_y(g_lock, -dy, 0);
    }
  else if (dy > UNLOCK_DRAG_DIST)
    {
      launcher_unlock();
    }
  else
    {
      launcher_anim(g_lock, launcher_translate_y_cb, -dy, 0, 0,
                    lv_anim_path_overshoot, NULL);
    }
}

/* The curtain lives on the top layer and follows the finger */

static void launcher_create_lock(void)
{
  lv_obj_t *lock = lv_obj_create(lv_layer_top());
  lv_obj_t *obj;
  lv_anim_t a;

  lv_obj_remove_style_all(lock);
  lv_obj_remove_flag(lock, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(lock, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_set_size(lock, LAUNCHER_SCREEN_W, LAUNCHER_SCREEN_H);
  lv_obj_set_style_bg_color(lock, lv_color_hex(LAUNCHER_LOCK_COLOR), 0);
  lv_obj_set_style_bg_grad_color(lock, lv_color_black(), 0);
  lv_obj_set_style_bg_grad_dir(lock, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(lock, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(lock, launcher_drag_event_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(lock, launcher_drag_event_cb, LV_EVENT_PRESSING,
                      NULL);
  lv_obj_add_event_cb(lock, launcher_drag_event_cb, LV_EVENT_RELEASED,
                      NULL);

  g_lock_clock = launcher_label(lock, "00:00:00", &lv_font_montserrat_48,
                                0xffffff, LV_ALIGN_TOP_MID, 0, 260);
  launcher_zoom_label(g_lock_clock, 50, 0);
  launcher_dim(launcher_label(lock, "openvela on LogicPi A1",
                              &lv_font_montserrat_24, 0xffffff,
                              LV_ALIGN_TOP_MID, 0, 420));

  obj = launcher_ring(lock, LV_SYMBOL_UP, launcher_unlock_event_cb);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_align(obj, LV_ALIGN_BOTTOM_MID, 0, -150);

  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_exec_cb(&a, launcher_translate_y_cb);
  lv_anim_set_values(&a, 0, -20);
  lv_anim_set_duration(&a, 600);
  lv_anim_set_playback_duration(&a, 600);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_start(&a);

  launcher_dim(launcher_label(lock, "Swipe up to unlock",
                              &lv_font_montserrat_24, 0xffffff,
                              LV_ALIGN_BOTTOM_MID, 0, -80));

  g_lock = lock;
}

/****************************************************************************
 * Name: lifecycle
 ****************************************************************************/

static void launcher_periodic_cb(lv_timer_t *timer)
{
  struct mallinfo mi = mallinfo();
  struct timespec ts;
  unsigned int sec;

  UNUSED(timer);

  clock_gettime(CLOCK_MONOTONIC, &ts);
  sec = ts.tv_sec;

  lv_label_set_text_fmt(g_lock_clock, "%02u:%02u:%02u",
                        (sec / 3600) % 100, (sec / 60) % 60, sec % 60);
  lv_label_set_text(g_home_clock, lv_label_get_text(g_lock_clock));
  lv_label_set_text_fmt(g_home_heap, "%u KB", mi.fordblks / 1024);
  lv_label_set_text_fmt(g_about_uptime, "%u s", sec);
  lv_label_set_text_fmt(g_about_heap, "%u KB", mi.fordblks / 1024);
}

/* "last" is the app that just exited, or -1 on boot */

static int launcher_start(int last)
{
  lv_nuttx_dsc_t info;
  lv_obj_t *obj;

  lv_init();

  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, &g_result);

  if (g_result.disp == NULL || g_result.indev == NULL)
    {
      fprintf(stderr, "launcher: no display or touchscreen\n");
      return -ENODEV;
    }

  lv_style_transition_dsc_init(&g_press_trans, g_press_props,
                               lv_anim_path_ease_out, 120, 0, NULL);

  launcher_create_about_scr();
  launcher_create_home_scr(last);
  launcher_create_lock();
  lv_screen_load(g_home_scr);

  launcher_periodic_cb(NULL);
  lv_timer_create(launcher_periodic_cb, 1000, NULL);
  lv_timer_create(launcher_flip_cb, LAUNCHER_FLIP_PERIOD, NULL);

  if (last < 0)
    {
      lv_obj_add_flag(g_home_tiles, LV_OBJ_FLAG_HIDDEN);
      return 0;
    }

  /* Shrink the app back into its tile */

  lv_obj_add_flag(g_lock, LV_OBJ_FLAG_HIDDEN);
  lv_obj_update_layout(g_home_scr);
  lv_obj_get_coords(g_return_tile, &g_zoom_area);

  obj = launcher_app_tile(lv_layer_top(), last, false);
  launcher_anim(obj, launcher_zoom_cb, LAUNCHER_ZOOM_MAX, 0, 0,
                lv_anim_path_ease_in_out, lv_obj_delete_anim_completed_cb);
  return 0;
}

static void launcher_stop(void)
{
  lv_nuttx_deinit(&g_result);
  lv_deinit();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  FAR const struct builtin_s *app;
  uint32_t idle;
  int last = -1;
  pid_t pid;

  UNUSED(argc);
  UNUSED(argv);

  if (lv_is_initialized())
    {
      fprintf(stderr, "launcher: LVGL is busy\n");
      return 1;
    }

  while (launcher_start(last) == 0)
    {
      g_launch = -1;
      while (g_launch < 0)
        {
          idle = lv_timer_handler();
          usleep(MAX(idle, 1) * 1000);
        }

      /* Keep the zoomed tile on screen while the app starts */

      lv_refr_now(NULL);
      launcher_stop();

      last = g_launch;
      app = builtin_for_index(last);
      pid = task_create(app->name, app->priority, app->stacksize, app->main,
                        NULL);
      if (pid < 0)
        {
          fprintf(stderr, "launcher: start %s failed: %d\n", app->name,
                  errno);
        }
      else
        {
          waitpid(pid, NULL, 0);
        }
    }

  launcher_stop();
  return 1;
}
