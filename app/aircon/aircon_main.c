/****************************************************************************
 * app/aircon/aircon_main.c
 *
 * Air conditioner
 *
 * Power, mode, target temperature and load power of a Mi Home AC partner
 * on the LAN, with the load of the last two minutes, a button that
 * switches it on and off and two that set the temperature.  The page turns
 * blue while the AC is on.  On the mock (app/mihome) it says so.
 *
 * A thread of its own talks to the device, every 2 s and at once after a
 * press, and reads the state after every command.  The LVGL thread only
 * shows what it last got, so a device that does not answer never blocks
 * the screen.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/param.h>

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

#include "lvx_ui.h"
#include "mihome.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define AIRCON_POLL           20      /* 100 ms steps between two polls */

#define AIRCON_GREEN          0x51cf66

#define AIRCON_STATUS_Y       263
#define AIRCON_TEMP_Y         400
#define AIRCON_MODE_Y         760
#define AIRCON_RULE_Y         950
#define AIRCON_LOAD_Y         1010
#define AIRCON_CHART_Y        1220
#define AIRCON_CHART_H        200
#define AIRCON_CHART_POINTS   60      /* 2 minutes */
#define AIRCON_BUTTON_Y       1500
#define AIRCON_BUTTON_SIZE    260
#define AIRCON_STEP_SIZE      116     /* Temperature + and - */
#define AIRCON_TEMP_MIN       16
#define AIRCON_TEMP_MAX       30

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum aircon_link_e
{
  AIRCON_CONNECTING,
  AIRCON_NOCONF,
  AIRCON_OFFLINE,
  AIRCON_ONLINE
};

/* What the thread last got, the values only count when online */

struct aircon_state_s
{
  enum aircon_link_e link;
  unsigned int seq;         /* Counts the polls */
  struct mihome_state_s dev;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const char * const g_link_text[] =
{
  "CONNECTING", "NOT CONFIGURED", "OFFLINE", "ONLINE"
};

static const uint32_t g_link_color[] =
{
  UI_MUTED, UI_MUTED, UI_ORANGE, AIRCON_GREEN
};

/* Shared with the thread */

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static struct aircon_state_s g_state;
static volatile int g_request;    /* 1 on, -1 off, 0 none */
static volatile int g_temp_request;   /* Degree C, 0 none */
static volatile bool g_running;

/* LVGL thread only */

static struct aircon_state_s g_shown;
static int g_shown_request;
static int g_shown_temp_request;
static bool g_shown_on;
static bool g_shown_mock;

static lv_obj_t *g_scr;
static lv_obj_t *g_back;
static lv_obj_t *g_dot;
static lv_obj_t *g_link;
static lv_obj_t *g_via;
static lv_obj_t *g_badge;
static lv_obj_t *g_note;
static lv_obj_t *g_temp;
static lv_obj_t *g_degree;
static lv_obj_t *g_celsius;
static lv_obj_t *g_temp_none;
static lv_obj_t *g_step[2];       /* -, + */
static lv_obj_t *g_mode;
static lv_obj_t *g_compressor;
static lv_obj_t *g_load;
static lv_obj_t *g_watt;
static lv_obj_t *g_peak;
static lv_obj_t *g_chart;
static lv_chart_series_t *g_series;
static lv_obj_t *g_button;
static lv_obj_t *g_icon;
static lv_obj_t *g_hint;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Thread: store a poll, and drop the requests it answered */

static void aircon_publish(int ret, FAR const struct mihome_state_s *dev,
                           int req, int temp)
{
  FAR char *p;

  pthread_mutex_lock(&g_lock);
  g_state.link = ret == 0 ? AIRCON_ONLINE :
                 ret == -ENOENT || ret == -EINVAL || ret == -ENOTSUP ?
                 AIRCON_NOCONF : AIRCON_OFFLINE;
  if (ret == 0)
    {
      g_state.dev = *dev;
      for (p = g_state.dev.mode; *p != '\0'; p++)
        {
          *p = toupper(*p);
        }
    }

  if (req != 0 && g_request == req)
    {
      g_request = 0;
    }
  else if (req == 0 && temp != 0 && g_temp_request == temp)
    {
      g_temp_request = 0;
    }

  g_state.seq++;
  pthread_mutex_unlock(&g_lock);
}

static FAR void *aircon_thread(FAR void *arg)
{
  struct mihome_state_s dev;
  struct mihome_s h;
  int temp;
  int req;
  int ret;
  int i;

  UNUSED(arg);

  /* mihome opens the device on the first call, and again on every call
   * until that works, so a config put there later is picked up
   */

  mihome_init(&h);
  while (g_running)
    {
      /* Whatever a command did, the page shows what the device says */

      req = g_request;
      temp = g_temp_request;
      if (req != 0)
        {
          mihome_power(&h, req > 0);
        }
      else if (temp != 0)
        {
          mihome_temp(&h, temp);
        }

      ret = mihome_get(&h, &dev);
      aircon_publish(ret, &dev, req, temp);

      for (i = 0; i < AIRCON_POLL && g_running && g_request == 0 &&
                  g_temp_request == 0; i++)
        {
          usleep(100 * 1000);
        }
    }

  mihome_close(&h);
  return NULL;
}

/****************************************************************************
 * Name: user interface
 ****************************************************************************/

static void aircon_bg_cb(void *var, int32_t v)
{
  lv_obj_set_style_bg_color(var, lv_color_mix(lv_color_hex(UI_BLUE),
                                              lv_color_hex(UI_INK), v), 0);
}

/* Put obj right of a number, dy below the top of its box */

static void aircon_after(lv_obj_t *obj, lv_obj_t *num, int32_t gap,
                         int32_t dy)
{
  lv_obj_update_layout(num);
  lv_obj_align_to(obj, num, LV_ALIGN_OUT_RIGHT_TOP, gap, dy);
}

/* The load of the last two minutes, the scale grows with the peak */

static void aircon_chart_add(int load)
{
  int32_t *y = lv_chart_get_y_array(g_chart, g_series);
  int32_t peak = load;
  int i;

  lv_chart_set_next_value(g_chart, g_series, load);
  for (i = 0; i < AIRCON_CHART_POINTS; i++)
    {
      if (y[i] != LV_CHART_POINT_NONE)
        {
          peak = MAX(peak, y[i]);
        }
    }

  lv_chart_set_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, 0,
                     MAX(100, (peak * 5 / 4 + 49) / 50 * 50));
  lv_label_set_text_fmt(g_peak, "PEAK %d W", (int)peak);
}

static void aircon_show(const struct aircon_state_s *s, int req,
                        int temp_req)
{
  bool online = s->link == AIRCON_ONLINE;
  bool mock = online && s->dev.mock;
  bool on = online && s->dev.on;
  bool has_temp = online && (s->dev.fields & MIHOME_TEMP) != 0;
  bool has_load = online && (s->dev.fields & MIHOME_LOAD) != 0;
  bool anim = g_shown.link == AIRCON_ONLINE;
  bool busy = req != 0 || temp_req != 0;
  int temp = temp_req != 0 ? temp_req : s->dev.temp;
  char text[16];
  int i;

  /* The mock never looks like the real device */

  lv_obj_set_style_bg_color(g_dot, lv_color_hex(mock ? UI_YELLOW :
                                                g_link_color[s->link]), 0);
  lv_label_set_text(g_link, mock ? "MOCK DEVICE" : g_link_text[s->link]);
  lv_obj_update_flag(g_badge, LV_OBJ_FLAG_HIDDEN, !mock);
  lv_obj_update_flag(g_via, LV_OBJ_FLAG_HIDDEN, mock);
  lv_obj_update_flag(g_note, LV_OBJ_FLAG_HIDDEN, s->link != AIRCON_NOCONF);

  /* Values the model has, or dashes.  A temperature being sent shows
   * at once.
   */

  lv_obj_update_flag(g_temp, LV_OBJ_FLAG_HIDDEN, !has_temp);
  lv_obj_update_flag(g_degree, LV_OBJ_FLAG_HIDDEN, !has_temp);
  lv_obj_update_flag(g_celsius, LV_OBJ_FLAG_HIDDEN, !has_temp);
  lv_obj_update_flag(g_temp_none, LV_OBJ_FLAG_HIDDEN, has_temp);
  if (has_temp)
    {
      snprintf(text, sizeof(text), "%d", temp);
      ui_num_set(g_temp, text, anim && temp != g_shown.dev.temp);
      aircon_after(g_degree, g_temp, 20, ui_ink_top(UI_OSWALD_300) + 4);
      aircon_after(g_celsius, g_temp, 80, ui_ink_top(UI_OSWALD_300) -
                   ui_ink_top(UI_OSWALD_120));
    }

  for (i = 0; i < 2; i++)
    {
      lv_obj_update_flag(g_step[i], LV_OBJ_FLAG_HIDDEN, !has_temp);
      lv_obj_set_state(g_step[i], LV_STATE_DISABLED, !on || req != 0 ||
                       temp == (i == 0 ? AIRCON_TEMP_MIN : AIRCON_TEMP_MAX));
    }

  lv_label_set_text(g_mode, online && (s->dev.fields & MIHOME_MODE) ?
                            s->dev.mode : "--");

  /* power only says what the partner sent by infrared, the load says if
   * the compressor runs
   */

  lv_label_set_text(g_compressor, !has_load || s->dev.run_load == 0 ? "--" :
                    !on ? "OFF" :
                    s->dev.load >= s->dev.run_load ? "RUNNING" : "WAITING");

  if (has_load)
    {
      snprintf(text, sizeof(text), "%d", s->dev.load);
      ui_num_set(g_load, text, anim && s->dev.load != g_shown.dev.load);

      /* No mock load in a curve of the real device and back */

      if (s->dev.mock != g_shown_mock)
        {
          lv_chart_set_all_value(g_chart, g_series, LV_CHART_POINT_NONE);
          g_shown_mock = s->dev.mock;
        }

      if (s->seq != g_shown.seq)
        {
          aircon_chart_add(s->dev.load);
        }
    }

  lv_obj_update_flag(g_load, LV_OBJ_FLAG_HIDDEN, !has_load);

  /* W on the baseline of the number */

  aircon_after(g_watt, g_load, 16, lv_font_get_line_height(UI_OSWALD_120) -
               UI_OSWALD_120->base_line -
               lv_font_get_line_height(UI_OSWALD_64) +
               UI_OSWALD_64->base_line);
  lv_obj_update_flag(g_watt, LV_OBJ_FLAG_HIDDEN, !has_load);

  /* Blue while on, the button shows what a tap does */

  if (on != g_shown_on)
    {
      ui_anim(g_scr, aircon_bg_cb, on ? 0 : 255, on ? 255 : 0, 400, 0, NULL);
      lv_obj_set_style_bg_color(g_back, lv_color_hex(on ? UI_BLUE_DIM :
                                                     UI_INK_2), 0);
      lv_obj_set_style_bg_color(g_button, lv_color_hex(on ? 0xffffff :
                                                       UI_ORANGE), 0);
      lv_obj_set_style_text_color(g_icon, lv_color_hex(on ? UI_BLUE :
                                                       0xffffff), 0);
      g_shown_on = on;
    }

  lv_obj_set_state(g_button, LV_STATE_DISABLED, !online || busy);
  lv_label_set_text(g_hint, busy ? "SENDING" :
                            !online ? "" :
                            on ? "TAP TO TURN OFF" : "TAP TO TURN ON");
  lv_obj_align(g_hint, LV_ALIGN_TOP_MID, 0,
               AIRCON_BUTTON_Y + AIRCON_BUTTON_SIZE + 48);

  g_shown = *s;
  g_shown.dev.temp = temp;
  g_shown_request = req;
  g_shown_temp_request = temp_req;
}

static void aircon_update_cb(lv_timer_t *timer)
{
  struct aircon_state_s s;
  int temp;
  int req;

  UNUSED(timer);

  pthread_mutex_lock(&g_lock);
  s = g_state;
  req = g_request;
  temp = g_temp_request;
  pthread_mutex_unlock(&g_lock);

  if (s.seq != g_shown.seq || req != g_shown_request ||
      temp != g_shown_temp_request)
    {
      aircon_show(&s, req, temp);
    }
}

static void aircon_button_cb(lv_event_t *e)
{
  UNUSED(e);

  pthread_mutex_lock(&g_lock);
  if (g_state.link == AIRCON_ONLINE && g_request == 0)
    {
      g_request = g_state.dev.on ? -1 : 1;
    }

  pthread_mutex_unlock(&g_lock);
  aircon_update_cb(NULL);
}

/* One degree down or up from what is shown, presses add up */

static void aircon_step_cb(lv_event_t *e)
{
  int step = lv_event_get_target(e) == g_step[0] ? -1 : 1;
  int temp;

  pthread_mutex_lock(&g_lock);
  if (g_state.link == AIRCON_ONLINE && g_state.dev.on && g_request == 0)
    {
      temp = g_temp_request != 0 ? g_temp_request : g_state.dev.temp;
      g_temp_request = MIN(MAX(temp + step, AIRCON_TEMP_MIN),
                           AIRCON_TEMP_MAX);
    }

  pthread_mutex_unlock(&g_lock);
  aircon_update_cb(NULL);
}

static void aircon_home_cb(lv_event_t *e)
{
  UNUSED(e);
  g_running = false;
}

/* Small spaced capitals, half white so they read on ink and on blue */

static lv_obj_t *aircon_tag(const char *text, int32_t x, int32_t y)
{
  lv_obj_t *tag = ui_tag(g_scr, 0xffffff, text);

  lv_obj_set_style_text_opa(tag, LV_OPA_60, 0);
  ui_place(tag, UI_OUTFIT_26, x, y);
  return tag;
}

static void aircon_create(void)
{
  lv_obj_t *obj;
  int32_t x2 = UI_MARGIN + UI_CONTENT_W / 2;
  int i;

  g_scr = lv_screen_active();
  lv_obj_remove_style_all(g_scr);
  lv_obj_remove_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(g_scr, lv_color_hex(UI_INK), 0);
  lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, 0);

  g_back = ui_back(g_scr, UI_INK_2, aircon_home_cb);
  lv_obj_set_pos(g_back, UI_MARGIN, 56);
  ui_place(ui_label(g_scr, UI_OSWALD_120, 0xffffff, "AIRCON"), UI_OSWALD_120,
           222, 50);

  /* Link to the device */

  g_dot = ui_box(g_scr, UI_MUTED, LV_RADIUS_CIRCLE);
  lv_obj_set_size(g_dot, 20, 20);
  lv_obj_set_pos(g_dot, UI_MARGIN, AIRCON_STATUS_Y + 1);
  g_link = aircon_tag("", UI_MARGIN + 36, AIRCON_STATUS_Y);
  lv_obj_set_style_text_opa(g_link, LV_OPA_COVER, 0);
  g_via = aircon_tag("MIIO OVER ETHERNET", 0, AIRCON_STATUS_Y);
  lv_obj_align(g_via, LV_ALIGN_TOP_RIGHT, -UI_MARGIN, lv_obj_get_y(g_via));
  g_badge = ui_demo_badge(g_scr);
  lv_obj_align(g_badge, LV_ALIGN_TOP_RIGHT, -UI_MARGIN,
               AIRCON_STATUS_Y - 24);

  g_note = ui_label(g_scr, UI_OUTFIT_26, 0xffffff,
                    "Put the device IP and token in " MIIO_CONF);
  lv_obj_set_style_text_opa(g_note, LV_OPA_60, 0);
  ui_place(g_note, UI_OUTFIT_26, UI_MARGIN, AIRCON_STATUS_Y + 50);

  /* Target temperature, the degree sign is a ring */

  aircon_tag("TARGET", UI_MARGIN, AIRCON_TEMP_Y - 60);
  g_temp = ui_num(g_scr, UI_OSWALD_300, 0xffffff);
  ui_num_set(g_temp, "0", false);
  ui_place(g_temp, UI_OSWALD_300, UI_MARGIN, AIRCON_TEMP_Y);

  g_degree = ui_box(g_scr, 0xffffff, LV_RADIUS_CIRCLE);
  lv_obj_set_style_bg_opa(g_degree, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(g_degree, lv_color_white(), 0);
  lv_obj_set_style_border_width(g_degree, 9, 0);
  lv_obj_set_size(g_degree, 44, 44);
  g_celsius = ui_label(g_scr, UI_OSWALD_120, 0xffffff, "C");

  g_temp_none = ui_label(g_scr, UI_OSWALD_120, 0xffffff, "--");
  ui_place(g_temp_none, UI_OSWALD_120, UI_MARGIN, AIRCON_TEMP_Y + 60);

  /* + above -, at the right edge, white glass on ink and on blue */

  for (i = 0; i < 2; i++)
    {
      g_step[i] = ui_box(g_scr, 0xffffff, LV_RADIUS_CIRCLE);
      lv_obj_add_flag(g_step[i], LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_size(g_step[i], AIRCON_STEP_SIZE, AIRCON_STEP_SIZE);
      lv_obj_align(g_step[i], LV_ALIGN_TOP_RIGHT, -UI_MARGIN,
                   AIRCON_TEMP_Y + (i == 0 ? 140 : 0));
      lv_obj_set_style_bg_opa(g_step[i], LV_OPA_20, 0);
      lv_obj_set_style_bg_opa(g_step[i], LV_OPA_40, LV_STATE_PRESSED);
      lv_obj_set_style_bg_opa(g_step[i], LV_OPA_10, LV_STATE_DISABLED);
      lv_obj_set_style_text_opa(g_step[i], LV_OPA_30, LV_STATE_DISABLED);
      lv_obj_add_event_cb(g_step[i], aircon_step_cb, LV_EVENT_CLICKED,
                          NULL);
      lv_obj_center(ui_label(g_step[i], UI_ICON_48, 0xffffff,
                             i == 0 ? LV_SYMBOL_MINUS : LV_SYMBOL_PLUS));
    }

  /* Mode and whether the compressor runs */

  aircon_tag("MODE", UI_MARGIN, AIRCON_MODE_Y);
  g_mode = ui_label(g_scr, UI_OSWALD_64, 0xffffff, "--");
  ui_place(g_mode, UI_OSWALD_64, UI_MARGIN, AIRCON_MODE_Y + 56);
  aircon_tag("COMPRESSOR", x2, AIRCON_MODE_Y);
  g_compressor = ui_label(g_scr, UI_OSWALD_64, 0xffffff, "--");
  ui_place(g_compressor, UI_OSWALD_64, x2, AIRCON_MODE_Y + 56);

  obj = ui_box(g_scr, 0xffffff, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_20, 0);
  lv_obj_set_pos(obj, UI_MARGIN, AIRCON_RULE_Y);
  lv_obj_set_size(obj, UI_CONTENT_W, 2);

  /* Load power now and over the last two minutes */

  aircon_tag("LOAD POWER", UI_MARGIN, AIRCON_LOAD_Y);
  g_peak = aircon_tag("", 0, AIRCON_LOAD_Y);
  lv_obj_align(g_peak, LV_ALIGN_TOP_RIGHT, -UI_MARGIN, lv_obj_get_y(g_peak));
  g_load = ui_num(g_scr, UI_OSWALD_120, 0xffffff);
  ui_num_set(g_load, "0", false);
  ui_place(g_load, UI_OSWALD_120, UI_MARGIN, AIRCON_LOAD_Y + 56);
  g_watt = ui_label(g_scr, UI_OSWALD_64, 0xffffff, "W");
  lv_obj_set_style_text_opa(g_watt, LV_OPA_60, 0);

  g_chart = lv_chart_create(g_scr);
  lv_obj_remove_style_all(g_chart);
  lv_obj_set_pos(g_chart, UI_MARGIN, AIRCON_CHART_Y);
  lv_obj_set_size(g_chart, UI_CONTENT_W, AIRCON_CHART_H);
  lv_obj_remove_flag(g_chart, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_line_width(g_chart, 5, LV_PART_ITEMS);
  lv_obj_set_style_line_rounded(g_chart, true, LV_PART_ITEMS);
  lv_obj_set_style_width(g_chart, 0, LV_PART_INDICATOR);
  lv_obj_set_style_height(g_chart, 0, LV_PART_INDICATOR);
  lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_div_line_count(g_chart, 0, 0);
  lv_chart_set_point_count(g_chart, AIRCON_CHART_POINTS);
  lv_chart_set_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  g_series = lv_chart_add_series(g_chart, lv_color_hex(UI_YELLOW),
                                 LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(g_chart, g_series, LV_CHART_POINT_NONE);

  obj = ui_box(g_scr, 0xffffff, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_20, 0);
  lv_obj_set_pos(obj, UI_MARGIN, AIRCON_CHART_Y + AIRCON_CHART_H + 8);
  lv_obj_set_size(obj, UI_CONTENT_W, 2);
  aircon_tag("2 MIN AGO", UI_MARGIN, AIRCON_CHART_Y + AIRCON_CHART_H + 32);
  obj = aircon_tag("NOW", 0, AIRCON_CHART_Y + AIRCON_CHART_H + 32);
  lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -UI_MARGIN, lv_obj_get_y(obj));

  /* Power button */

  g_button = ui_box(g_scr, UI_ORANGE, LV_RADIUS_CIRCLE);
  lv_obj_add_flag(g_button, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(g_button, AIRCON_BUTTON_SIZE, AIRCON_BUTTON_SIZE);
  lv_obj_align(g_button, LV_ALIGN_TOP_MID, 0, AIRCON_BUTTON_Y);
  lv_obj_set_style_bg_opa(g_button, LV_OPA_70, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(g_button, LV_OPA_30, LV_STATE_DISABLED);
  lv_obj_add_event_cb(g_button, aircon_button_cb, LV_EVENT_CLICKED, NULL);
  g_icon = ui_label(g_button, UI_ICON_48, 0xffffff, LV_SYMBOL_POWER);
  lv_obj_center(g_icon);

  g_hint = aircon_tag("", 0, 0);

  g_shown.seq = -1;
  aircon_show(&g_state, 0, 0);
  lv_timer_create(aircon_update_cb, 100, NULL);
}

static int aircon_start(lv_nuttx_result_t *result)
{
  lv_nuttx_dsc_t info;

  lv_init();

  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, result);

  if (result->disp == NULL || result->indev == NULL)
    {
      fprintf(stderr, "aircon: no display or touchscreen\n");
      return -1;
    }

  ui_hide_perf();

  aircon_create();
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  lv_nuttx_result_t result;
  pthread_attr_t attr;
  pthread_t thread;
  uint32_t idle;

  UNUSED(argc);
  UNUSED(argv);

  /* LVGL is one global instance, shared by every task of the flat build */

  if (lv_is_initialized())
    {
      fprintf(stderr, "aircon: LVGL is busy, start it from the "
              "launcher\n");
      return 1;
    }

  memset(&g_state, 0, sizeof(g_state));
  memset(&g_shown, 0, sizeof(g_shown));
  g_request = 0;
  g_temp_request = 0;
  g_shown_request = 0;
  g_shown_temp_request = 0;
  g_shown_on = false;
  g_shown_mock = false;

  g_running = aircon_start(&result) == 0;
  if (!g_running)
    {
      lv_nuttx_deinit(&result);
      lv_deinit();
      return 1;
    }

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 8192);
  if (pthread_create(&thread, &attr, aircon_thread, NULL) != 0)
    {
      fprintf(stderr, "aircon: no thread\n");
      g_running = false;
    }
  else
    {
      while (g_running)
        {
          idle = lv_timer_handler();
          usleep(MAX(idle, 1) * 1000);
        }

      /* The thread sees g_running within 100 ms, or after at most two
       * 2 s timeouts while the device does not answer
       */

      pthread_join(thread, NULL);
    }

  lv_nuttx_deinit(&result);
  lv_deinit();
  return 0;
}
