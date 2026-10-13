/****************************************************************************
 * app/bench/bench_main.c
 *
 * LVGL benchmark app
 * CPU / memory throughput, display refresh rate and screen test patterns
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BENCH_SCREEN_W        1080
#define BENCH_SCREEN_H        1920

#define BENCH_BG_TOP          0x16243a
#define BENCH_BG_BOTTOM       0x080d16
#define BENCH_CARD_COLOR      0x1c2b42
#define BENCH_CARD_PRESSED    0x2a3d5a
#define BENCH_ACCENT          0x00a8f3
#define BENCH_ACCENT_PRESSED  0x0086c2
#define BENCH_TEXT_DIM        0x8fa3bf

#define BENCH_TIME_MS         500
#define BENCH_INT_LOOPS       1000000
#define BENCH_MATRIX_N        64
#define BENCH_MEM_SIZE        (4 * 1024 * 1024)
#define BENCH_FRAMES          60
#define BENCH_GRID_STEP       120
#define BENCH_PATTERNS        (nitems(g_pattern_colors) + 2)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bench_test_s
{
  const char *name;
  void (*run)(void);
  uint32_t work;            /* Operations or bytes per run() */
  const char *unit;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void bench_int(void);
static void bench_float(void);
static void bench_memcpy(void);
static void bench_memset(void);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct bench_test_s g_tests[] =
{
  { "Integer (xorshift)", bench_int, 6 * BENCH_INT_LOOPS, "MOPS" },
  { "Float (matmul)", bench_float,
    2 * BENCH_MATRIX_N * BENCH_MATRIX_N * BENCH_MATRIX_N, "MFLOPS" },
  { "Memory copy", bench_memcpy, BENCH_MEM_SIZE, "MB/s" },
  { "Memory fill", bench_memset, BENCH_MEM_SIZE, "MB/s" },
};

static const uint32_t g_pattern_colors[] =
{
  0xff0000, 0x00ff00, 0x0000ff, 0xffffff, 0x000000
};

static float g_mat_a[BENCH_MATRIX_N][BENCH_MATRIX_N];
static float g_mat_b[BENCH_MATRIX_N][BENCH_MATRIX_N];
static float g_mat_c[BENCH_MATRIX_N][BENCH_MATRIX_N];
static volatile uint32_t g_sink;
static uint8_t *g_mem;

static lv_obj_t *g_cpu_value[nitems(g_tests)];
static lv_obj_t *g_fill_value;
static lv_obj_t *g_blend_value;
static int g_pattern;
static bool g_running;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t bench_ms(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/****************************************************************************
 * Name: CPU and memory tests
 ****************************************************************************/

static void bench_int(void)
{
  uint32_t x = g_sink | 1;
  int i;

  for (i = 0; i < BENCH_INT_LOOPS; i++)
    {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
    }

  g_sink = x;
}

static void bench_float(void)
{
  int i;
  int j;
  int k;

  for (i = 0; i < BENCH_MATRIX_N; i++)
    {
      for (j = 0; j < BENCH_MATRIX_N; j++)
        {
          float sum = 0.0f;

          for (k = 0; k < BENCH_MATRIX_N; k++)
            {
              sum += g_mat_a[i][k] * g_mat_b[k][j];
            }

          g_mat_c[i][j] = sum;
        }
    }

  g_sink = (uint32_t)g_mat_c[BENCH_MATRIX_N - 1][BENCH_MATRIX_N - 1];
}

static void bench_memcpy(void)
{
  memcpy(g_mem + BENCH_MEM_SIZE, g_mem, BENCH_MEM_SIZE);
}

static void bench_memset(void)
{
  memset(g_mem, g_sink, BENCH_MEM_SIZE);
}

/* Repeat a test for BENCH_TIME_MS, return millions of work units/s */

static uint32_t bench_cpu(const struct bench_test_s *t)
{
  uint32_t start = bench_ms();
  uint32_t elapsed;
  uint64_t runs = 0;

  do
    {
      t->run();
      runs++;
      elapsed = bench_ms() - start;
    }
  while (elapsed < BENCH_TIME_MS);

  return runs * t->work / elapsed / 1000;
}

/****************************************************************************
 * Name: display tests
 ****************************************************************************/

static lv_obj_t *bench_rect(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t w, int32_t h)
{
  lv_obj_t *obj = lv_obj_create(parent);

  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  return obj;
}

/* Redraw the whole screen BENCH_FRAMES times through a top layer rect */

static void bench_refresh(lv_obj_t *label, lv_opa_t opa)
{
  lv_obj_t *obj = bench_rect(lv_layer_top(), 0, 0, BENCH_SCREEN_W,
                             BENCH_SCREEN_H);
  uint32_t start = bench_ms();
  uint32_t elapsed;
  int i;

  lv_obj_set_style_bg_opa(obj, opa, 0);

  for (i = 0; i < BENCH_FRAMES; i++)
    {
      lv_obj_set_style_bg_color(obj,
        lv_color_hsv_to_rgb(i * 360 / BENCH_FRAMES, 70, 90), 0);
      lv_refr_now(NULL);
    }

  elapsed = bench_ms() - start;
  lv_obj_delete(obj);

  lv_label_set_text_fmt(label, "%u fps, %u Mpx/s",
                        BENCH_FRAMES * 1000 / elapsed,
                        (uint32_t)((uint64_t)BENCH_FRAMES * BENCH_SCREEN_W *
                                   BENCH_SCREEN_H / elapsed / 1000));
}

static void bench_pattern_show(lv_obj_t *overlay)
{
  static const uint32_t ramps[] =
  {
    0xff0000, 0x00ff00, 0x0000ff, 0xffffff
  };

  lv_obj_t *obj;
  int i;

  lv_obj_clean(overlay);
  lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);

  if (g_pattern < nitems(g_pattern_colors))
    {
      /* Solid colors to spot dead or stuck pixels */

      lv_obj_set_style_bg_color(overlay,
                                lv_color_hex(g_pattern_colors[g_pattern]),
                                0);
    }
  else if (g_pattern == nitems(g_pattern_colors))
    {
      /* Black to full intensity ramps to check color depth and banding */

      for (i = 0; i < nitems(ramps); i++)
        {
          obj = bench_rect(overlay, 0, i * BENCH_SCREEN_H / nitems(ramps),
                           BENCH_SCREEN_W, BENCH_SCREEN_H / nitems(ramps));
          lv_obj_set_style_bg_color(obj, lv_color_black(), 0);
          lv_obj_set_style_bg_grad_color(obj, lv_color_hex(ramps[i]), 0);
          lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, 0);
        }
    }
  else
    {
      /* 1px grid, including the outermost rows and columns */

      for (i = 0; i <= BENCH_SCREEN_W; i += BENCH_GRID_STEP)
        {
          bench_rect(overlay, MIN(i, BENCH_SCREEN_W - 1), 0, 1,
                     BENCH_SCREEN_H);
        }

      for (i = 0; i <= BENCH_SCREEN_H; i += BENCH_GRID_STEP)
        {
          bench_rect(overlay, 0, MIN(i, BENCH_SCREEN_H - 1),
                     BENCH_SCREEN_W, 1);
        }
    }
}

static void bench_pattern_cb(lv_event_t *e)
{
  lv_obj_t *overlay = lv_event_get_target(e);

  if (++g_pattern < BENCH_PATTERNS)
    {
      bench_pattern_show(overlay);
    }
  else
    {
      lv_obj_delete_async(overlay);
    }
}

/****************************************************************************
 * Name: user interface
 ****************************************************************************/

static void bench_home_cb(lv_event_t *e)
{
  UNUSED(e);
  g_running = false;
}

static void bench_cpu_cb(lv_event_t *e)
{
  int i;

  UNUSED(e);

  for (i = 0; i < nitems(g_tests); i++)
    {
      lv_label_set_text(g_cpu_value[i], "Running...");
      lv_refr_now(NULL);
      lv_label_set_text_fmt(g_cpu_value[i], "%u %s",
                            bench_cpu(&g_tests[i]), g_tests[i].unit);
    }
}

static void bench_display_cb(lv_event_t *e)
{
  UNUSED(e);
  bench_refresh(g_fill_value, LV_OPA_COVER);
  bench_refresh(g_blend_value, LV_OPA_50);
}

static void bench_patterns_cb(lv_event_t *e)
{
  lv_obj_t *overlay = bench_rect(lv_layer_top(), 0, 0, BENCH_SCREEN_W,
                                 BENCH_SCREEN_H);

  UNUSED(e);

  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(overlay, bench_pattern_cb, LV_EVENT_CLICKED, NULL);
  g_pattern = 0;
  bench_pattern_show(overlay);
}

static lv_obj_t *bench_label(lv_obj_t *parent, const char *text,
                             const lv_font_t *font, uint32_t color)
{
  lv_obj_t *label = lv_label_create(parent);

  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_label_set_text(label, text);
  return label;
}

static lv_obj_t *bench_box(lv_obj_t *parent, uint32_t color,
                           uint32_t pressed, int32_t radius)
{
  lv_obj_t *obj = lv_obj_create(parent);

  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_color(obj, lv_color_hex(pressed), LV_STATE_PRESSED);
  lv_obj_set_style_radius(obj, radius, 0);
  return obj;
}

static void bench_button(lv_obj_t *parent, const char *text,
                         lv_event_cb_t cb)
{
  lv_obj_t *btn = bench_box(parent, BENCH_ACCENT, BENCH_ACCENT_PRESSED, 32);

  lv_obj_set_size(btn, lv_pct(100), 110);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
  lv_obj_center(bench_label(btn, text, &lv_font_montserrat_24, 0xffffff));
}

static lv_obj_t *bench_row(lv_obj_t *parent, const char *key)
{
  lv_obj_t *row = lv_obj_create(parent);

  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  bench_label(row, key, &lv_font_montserrat_24, BENCH_TEXT_DIM);
  return bench_label(row, "-", &lv_font_montserrat_24, 0xffffff);
}

static lv_obj_t *bench_card(lv_obj_t *parent, const char *title)
{
  lv_obj_t *card = bench_box(parent, BENCH_CARD_COLOR, BENCH_CARD_COLOR,
                             40);

  lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(card, 48, 0);
  lv_obj_set_style_pad_row(card, 32, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

  bench_label(card, title, &lv_font_montserrat_24, BENCH_ACCENT);
  return card;
}

static void bench_create(void)
{
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *body;
  lv_obj_t *card;
  lv_obj_t *obj;
  int i;

  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr, lv_color_hex(BENCH_BG_TOP), 0);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(BENCH_BG_BOTTOM), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);

  obj = bench_box(scr, BENCH_CARD_COLOR, BENCH_CARD_PRESSED,
                  LV_RADIUS_CIRCLE);
  lv_obj_set_size(obj, 110, 110);
  lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 60, 60);
  lv_obj_add_event_cb(obj, bench_home_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_center(bench_label(obj, LV_SYMBOL_HOME, &lv_font_montserrat_24,
                            0xffffff));

  obj = bench_label(scr, "Benchmark", &lv_font_montserrat_48, 0xffffff);
  lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 210, 86);

  body = lv_obj_create(scr);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, 960, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(body, 40, 0);
  lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 230);

  card = bench_card(body, "CPU & MEMORY");
  for (i = 0; i < nitems(g_tests); i++)
    {
      g_cpu_value[i] = bench_row(card, g_tests[i].name);
    }

  bench_button(card, LV_SYMBOL_PLAY "  Run", bench_cpu_cb);

  card = bench_card(body, "DISPLAY");
  lv_label_set_text_fmt(bench_row(card, "Panel"), "%d x %d, %d bpp",
                        (int)lv_display_get_horizontal_resolution(NULL),
                        (int)lv_display_get_vertical_resolution(NULL),
                        lv_color_format_get_bpp(
                          lv_display_get_color_format(NULL)));
  g_fill_value = bench_row(card, "Full screen fill");
  g_blend_value = bench_row(card, "Full screen blend");
  bench_button(card, LV_SYMBOL_REFRESH "  Refresh rate", bench_display_cb);
  bench_button(card, LV_SYMBOL_IMAGE "  Test patterns", bench_patterns_cb);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;
  uint32_t idle;
  int i;

  UNUSED(argc);
  UNUSED(argv);

  /* LVGL is one global instance, shared by every task of the flat build */

  if (lv_is_initialized())
    {
      fprintf(stderr, "bench: LVGL is busy, start it from the launcher\n");
      return 1;
    }

  g_mem = malloc(2 * BENCH_MEM_SIZE);
  if (g_mem == NULL)
    {
      fprintf(stderr, "bench: memory alloc failed\n");
      return 1;
    }

  for (i = 0; i < BENCH_MATRIX_N * BENCH_MATRIX_N; i++)
    {
      g_mat_a[i / BENCH_MATRIX_N][i % BENCH_MATRIX_N] = i % 7;
      g_mat_b[i / BENCH_MATRIX_N][i % BENCH_MATRIX_N] = i % 5;
    }

  lv_init();

  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, &result);

  if (result.disp == NULL || result.indev == NULL)
    {
      fprintf(stderr, "bench: no display or touchscreen\n");
      g_running = false;
    }
  else
    {
      bench_create();
      g_running = true;
    }

  while (g_running)
    {
      idle = lv_timer_handler();
      usleep(MAX(idle, 1) * 1000);
    }

  lv_nuttx_deinit(&result);
  lv_deinit();
  free(g_mem);
  return 0;
}
