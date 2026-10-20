#include <pebble.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define APP_BLUE GColorVividCerulean
#define DEFAULT_WPM 20
#define MIN_WPM 5
#define MAX_WPM 35
#define DEFAULT_TONE_HZ 600
#define MIN_TONE_HZ 200
#define MAX_TONE_HZ 1500
#define TONE_HZ_STEP 50
#define TONE_VOLUME 60
#define TX_TEXT_CAP 96
#define TX_CODE_CAP 12
#define TX_VIBE_MAX 10000u

#define PERSIST_WPM_KEY 101
#define PERSIST_REVERSE_KEY 102
#define PERSIST_KEYING_MODE_KEY 103
#define PERSIST_STRAIGHT_BUTTON_KEY 104
#define PERSIST_TONE_HZ_KEY 105
#define PERSIST_TONE_ENABLED_KEY 106
#define PERSIST_VIBRATION_ENABLED_KEY 107

#define SPEED_ANIM_DISTANCE 42
#define SPEED_ANIM_DURATION 180

#define KEYER_FRAME_HOME 0
#define KEYER_FRAME_READY 8
#define KEYER_FRAME_DOWN 13
#define KEYER_FRAME_COUNT 14
#define KEYER_STEP_PRESS_MS 8
#define KEYER_STEP_APPROACH_MS 33
#define KEYER_STEP_RETURN_MS 45
#define KEYER_IDLE_RETURN_MS 1200

typedef struct {
  char letter;
  const char *code;
} CwPair;

static const CwPair s_alphabet[] = {
  {'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."},
  {'E', "."}, {'F', "..-."}, {'G', "--."}, {'H', "...."},
  {'I', ".."}, {'J', ".---"}, {'K', "-.-"}, {'L', ".-.."},
  {'M', "--"}, {'N', "-."}, {'O', "---"}, {'P', ".--."},
  {'Q', "--.-"}, {'R', ".-."}, {'S', "..."}, {'T', "-"},
  {'U', "..-"}, {'V', "...-"}, {'W', ".--"}, {'X', "-..-"},
  {'Y', "-.--"}, {'Z', "--.."}, {'0', "-----"}, {'1', ".----"},
  {'2', "..---"}, {'3', "...--"}, {'4', "....-"}, {'5', "....."},
  {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."},
  {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."},
  {'/', "-..-."}, {':', "---..."}, {'=', "-...-"},
  {'+', ".-.-."}, {'-', "-....-"}, {'@', ".--.-."},
  {'\'', ".----."}
};

typedef struct {
  uint16_t wpm;
  bool reverse_paddles;
  bool straight_key_mode;
  bool straight_key_uses_up;
  uint16_t tone_hz;
  bool tone_enabled;
  bool vibration_enabled;
} Settings;

static Settings s_settings;

// Main keyer window
static Window *s_main_window;
static TextLayer *s_message_layer;
static BitmapLayer *s_art_bitmap_layer;
static Layer *s_rail_layer;

typedef enum {
  KEYER_ANIM_IDLE = 0,
  KEYER_ANIM_APPROACH,
  KEYER_ANIM_PRESS,
  KEYER_ANIM_HOLD,
  KEYER_ANIM_RELEASE,
  KEYER_ANIM_RETURN
} KeyerAnimPhase;

static GBitmap *s_keyer_bitmap;

static const uint32_t s_keyer_frame_resources[KEYER_FRAME_COUNT] = {
  RESOURCE_ID_KEYER_FRAME_001,
  RESOURCE_ID_KEYER_FRAME_002,
  RESOURCE_ID_KEYER_FRAME_003,
  RESOURCE_ID_KEYER_FRAME_004,
  RESOURCE_ID_KEYER_FRAME_005,
  RESOURCE_ID_KEYER_FRAME_006,
  RESOURCE_ID_KEYER_FRAME_007,
  RESOURCE_ID_KEYER_FRAME_008,
  RESOURCE_ID_KEYER_FRAME_009,
  RESOURCE_ID_KEYER_FRAME_010,
  RESOURCE_ID_KEYER_FRAME_011,
  RESOURCE_ID_KEYER_FRAME_012,
  RESOURCE_ID_KEYER_FRAME_013,
  RESOURCE_ID_KEYER_FRAME_014
};

static uint8_t s_keyer_frame = KEYER_FRAME_HOME;
static uint8_t s_keyer_target = KEYER_FRAME_HOME;
static KeyerAnimPhase s_keyer_phase = KEYER_ANIM_IDLE;
static bool s_keyer_release_requested;
static AppTimer *s_keyer_anim_timer;
static AppTimer *s_keyer_idle_timer;

// Settings menu
static Window *s_menu_window;
static MenuLayer *s_menu_layer;
static MenuLayerCallbacks s_menu_callbacks;

// Speed editor
static Window *s_speed_window;
static TextLayer *s_speed_text_layer;
static char s_speed_text[16];
static char s_speed_next_text[16];
static int16_t s_speed_anim_direction;

// Tone-frequency editor
static Window *s_tone_window;
static TextLayer *s_tone_text_layer;
static char s_tone_text[20];

// Vibration battery warning
static Window *s_vibration_warning_window;
static TextLayer *s_vibration_warning_text_layer;

// Paddle choice dialog
static Window *s_paddle_window;
static TextLayer *s_paddle_prompt_layer;
static ActionBarLayer *s_paddle_action_bar;
static GBitmap *s_dit_icon;
static GBitmap *s_dah_icon;

// Keyer timers
static AppTimer *s_mark_timer;
static AppTimer *s_gap_timer;
static AppTimer *s_letter_timer;
static AppTimer *s_word_timer;

static bool s_active;
static bool s_up_held;
static bool s_down_held;
static bool s_up_pending;
static bool s_down_pending;
static bool s_paddle_mark;
static bool s_straight_held;
static uint32_t s_straight_press_start;
static int8_t s_last_paddle;
static char s_mark_symbol;
static uint16_t s_unit_ms;

#define DISPLAY_LINE_COUNT 3
#define DISPLAY_TEXT_CAP (DISPLAY_LINE_COUNT * (TX_TEXT_CAP + 1))

// Newest decoded line is row 1; completed rows move down on overflow.
static char s_lines[DISPLAY_LINE_COUNT][TX_TEXT_CAP + 1];
static size_t s_line_lengths[DISPLAY_LINE_COUNT];
static char s_display_text[DISPLAY_TEXT_CAP];
static int16_t s_message_width;
static char s_code[TX_CODE_CAP + 1];
static uint8_t s_code_length;
static uint32_t s_last_mark_end;
static uint32_t s_vibration_duration;

static uint32_t now_ms(void) {
  time_t seconds = 0;
  uint16_t millis = 0;
  time_ms(&seconds, &millis);
  return (uint32_t)seconds * 1000u + millis;
}

static void cancel_timer(AppTimer **timer) {
  if (*timer) {
    app_timer_cancel(*timer);
    *timer = NULL;
  }
}

static void keyer_anim_cancel(AppTimer **timer) {
  if (*timer) {
    app_timer_cancel(*timer);
    *timer = NULL;
  }
}

static void keyer_anim_step(void *context);
static void keyer_idle_return(void *context);

static bool keyer_bitmap_show_frame(uint8_t frame_index) {
  if (frame_index >= KEYER_FRAME_COUNT || !s_art_bitmap_layer) {
    return false;
  }

  GBitmap *next =
      gbitmap_create_with_resource(s_keyer_frame_resources[frame_index]);
  if (!next) {
    return false;
  }

  GBitmap *old = s_keyer_bitmap;
  s_keyer_bitmap = next;

  bitmap_layer_set_bitmap(s_art_bitmap_layer, s_keyer_bitmap);
  layer_mark_dirty(bitmap_layer_get_layer(s_art_bitmap_layer));

  if (old) {
    gbitmap_destroy(old);
  }

  return true;
}

static void keyer_anim_draw_frame(uint8_t frame_index) {
  if (frame_index >= KEYER_FRAME_COUNT) return;

  s_keyer_frame = frame_index;

  if (!keyer_bitmap_show_frame(frame_index)) {
    APP_LOG(APP_LOG_LEVEL_ERROR,
            "Could not load keyer frame %u",
            (unsigned)(frame_index + 1));
  }
}

static uint32_t keyer_anim_interval(void) {
  switch (s_keyer_phase) {
    case KEYER_ANIM_APPROACH:
      return KEYER_STEP_APPROACH_MS;
    case KEYER_ANIM_RETURN:
      return KEYER_STEP_RETURN_MS;
    case KEYER_ANIM_PRESS:
    case KEYER_ANIM_RELEASE:
      return KEYER_STEP_PRESS_MS;
    default:
      return KEYER_STEP_PRESS_MS;
  }
}

static void keyer_anim_start(KeyerAnimPhase phase, uint8_t target) {
  if (target >= KEYER_FRAME_COUNT) return;

  s_keyer_phase = phase;
  s_keyer_target = target;

  keyer_anim_cancel(&s_keyer_anim_timer);
  if (s_keyer_frame != s_keyer_target) {
    s_keyer_anim_timer =
        app_timer_register(keyer_anim_interval(), keyer_anim_step, NULL);
  }
}

static void keyer_anim_schedule_home(void) {
  keyer_anim_cancel(&s_keyer_idle_timer);
  s_keyer_idle_timer =
      app_timer_register(KEYER_IDLE_RETURN_MS, keyer_idle_return, NULL);
}

static void keyer_anim_step(void *context) {
  (void)context;
  s_keyer_anim_timer = NULL;

  if (s_keyer_frame < s_keyer_target) {
    keyer_anim_draw_frame(s_keyer_frame + 1);
  } else if (s_keyer_frame > s_keyer_target) {
    keyer_anim_draw_frame(s_keyer_frame - 1);
  }

  if (s_keyer_frame != s_keyer_target) {
    s_keyer_anim_timer =
        app_timer_register(keyer_anim_interval(), keyer_anim_step, NULL);
    return;
  }

  switch (s_keyer_phase) {
    case KEYER_ANIM_APPROACH:
      // HTML preview: 1 -> 9, then immediately 10 -> 14.
      keyer_anim_start(KEYER_ANIM_PRESS, KEYER_FRAME_DOWN);
      break;

    case KEYER_ANIM_PRESS:
      if (s_keyer_release_requested) {
        keyer_anim_start(KEYER_ANIM_RELEASE, KEYER_FRAME_READY);
      } else {
        s_keyer_phase = KEYER_ANIM_HOLD;
      }
      break;

    case KEYER_ANIM_RELEASE:
      s_keyer_phase = KEYER_ANIM_HOLD;
      keyer_anim_schedule_home();
      break;

    case KEYER_ANIM_RETURN:
      s_keyer_phase = KEYER_ANIM_IDLE;
      break;

    default:
      break;
  }
}

static void keyer_idle_return(void *context) {
  (void)context;
  s_keyer_idle_timer = NULL;

  // HTML preview: poised frame 9 returns through 8..1.
  if (s_keyer_frame != KEYER_FRAME_HOME) {
    keyer_anim_start(KEYER_ANIM_RETURN, KEYER_FRAME_HOME);
  } else {
    s_keyer_phase = KEYER_ANIM_IDLE;
  }
}

static void keyer_anim_press(void) {
  keyer_anim_cancel(&s_keyer_idle_timer);
  s_keyer_release_requested = false;

  if (s_keyer_frame == KEYER_FRAME_HOME) {
    // First activity: preserve the complete bird approach before the key press.
    keyer_anim_start(KEYER_ANIM_APPROACH, KEYER_FRAME_READY);
  } else {
    // Already at/near the sounder: drive toward the fully pressed frame.
    keyer_anim_start(KEYER_ANIM_PRESS, KEYER_FRAME_DOWN);
  }
}

static void keyer_anim_release(void) {
  s_keyer_release_requested = true;

  // Never truncate the approach/press sequence. A short dit may finish before
  // frame 14; in that case the animation completes 9 -> 14, then 13 -> 9.
  if (s_keyer_phase == KEYER_ANIM_HOLD ||
      (s_keyer_phase == KEYER_ANIM_PRESS &&
       s_keyer_frame == KEYER_FRAME_DOWN)) {
    keyer_anim_start(KEYER_ANIM_RELEASE, KEYER_FRAME_READY);
  }
}

static void keyer_anim_reset(void) {
  keyer_anim_cancel(&s_keyer_anim_timer);
  keyer_anim_cancel(&s_keyer_idle_timer);
  s_keyer_phase = KEYER_ANIM_IDLE;
  s_keyer_release_requested = false;
  s_keyer_target = KEYER_FRAME_HOME;
  keyer_anim_draw_frame(KEYER_FRAME_HOME);
}

static void settings_load(void) {
  s_settings.wpm = DEFAULT_WPM;
  s_settings.reverse_paddles = false;
  s_settings.straight_key_mode = false;
  s_settings.straight_key_uses_up = true;
  s_settings.tone_hz = DEFAULT_TONE_HZ;
  s_settings.tone_enabled = true;
  s_settings.vibration_enabled = false;

  if (persist_exists(PERSIST_WPM_KEY)) {
    int value = persist_read_int(PERSIST_WPM_KEY);
    if (value >= MIN_WPM && value <= MAX_WPM) {
      s_settings.wpm = (uint16_t)value;
    }
  }
  if (persist_exists(PERSIST_REVERSE_KEY)) {
    s_settings.reverse_paddles = persist_read_bool(PERSIST_REVERSE_KEY);
  }
  if (persist_exists(PERSIST_KEYING_MODE_KEY)) {
    s_settings.straight_key_mode =
        persist_read_bool(PERSIST_KEYING_MODE_KEY);
  }
  if (persist_exists(PERSIST_STRAIGHT_BUTTON_KEY)) {
    s_settings.straight_key_uses_up =
        persist_read_bool(PERSIST_STRAIGHT_BUTTON_KEY);
  }
  if (persist_exists(PERSIST_TONE_HZ_KEY)) {
    int value = persist_read_int(PERSIST_TONE_HZ_KEY);
    if (value >= MIN_TONE_HZ && value <= MAX_TONE_HZ) {
      s_settings.tone_hz = (uint16_t)value;
    }
  }
  if (persist_exists(PERSIST_TONE_ENABLED_KEY)) {
    s_settings.tone_enabled =
        persist_read_bool(PERSIST_TONE_ENABLED_KEY);
  }
  if (persist_exists(PERSIST_VIBRATION_ENABLED_KEY)) {
    s_settings.vibration_enabled =
        persist_read_bool(PERSIST_VIBRATION_ENABLED_KEY);
  }

  s_unit_ms = (uint16_t)(1200u / s_settings.wpm);
}

static void settings_save(void) {
  persist_write_int(PERSIST_WPM_KEY, s_settings.wpm);
  persist_write_bool(PERSIST_REVERSE_KEY, s_settings.reverse_paddles);
  persist_write_bool(PERSIST_KEYING_MODE_KEY, s_settings.straight_key_mode);
  persist_write_bool(PERSIST_STRAIGHT_BUTTON_KEY,
                     s_settings.straight_key_uses_up);
  persist_write_int(PERSIST_TONE_HZ_KEY, s_settings.tone_hz);
  persist_write_bool(PERSIST_TONE_ENABLED_KEY, s_settings.tone_enabled);
  persist_write_bool(PERSIST_VIBRATION_ENABLED_KEY,
                     s_settings.vibration_enabled);
}

static void refresh_timing(void) {
  s_unit_ms = (uint16_t)(1200u / s_settings.wpm);
  if (!s_unit_ms) s_unit_ms = 1;
}

static void draw_message(void) {
  size_t used = 0;
  s_display_text[0] = '\0';

  for (int row = 0; row < DISPLAY_LINE_COUNT; ++row) {
    if (row > 0) s_display_text[used++] = '\n';
    const size_t length = s_line_lengths[row];
    memcpy(s_display_text + used, s_lines[row], length);
    used += length;
  }
  s_display_text[used] = '\0';

  if (s_message_layer) {
    text_layer_set_text(s_message_layer, s_display_text);
  }
}

static int16_t measured_text_width(const char *text) {
  const GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  const GSize size = graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, 1000, 60),
      GTextOverflowModeWordWrap, GTextAlignmentLeft);
  return size.w;
}

static void scroll_lines_down(void) {
  for (int row = DISPLAY_LINE_COUNT - 1; row > 0; --row) {
    memcpy(s_lines[row], s_lines[row - 1], sizeof(s_lines[row]));
    s_line_lengths[row] = s_line_lengths[row - 1];
  }
  s_lines[0][0] = '\0';
  s_line_lengths[0] = 0;
}

static void append_text(char value) {
  // Do not start a new line with a space. If the current line is full,
  // the next actual character triggers the downward shift.
  if (value == ' ' &&
      (s_line_lengths[0] == 0 ||
       s_lines[0][s_line_lengths[0] - 1] == ' ')) {
    return;
  }

  char candidate[TX_TEXT_CAP + 1];
  const size_t current_length = s_line_lengths[0];
  if (current_length < TX_TEXT_CAP) {
    memcpy(candidate, s_lines[0], current_length);
    candidate[current_length] = value;
    candidate[current_length + 1] = '\0';
  } else {
    candidate[0] = '\0';
  }

  if (current_length >= TX_TEXT_CAP ||
      measured_text_width(candidate) > s_message_width) {
    if (value == ' ') return;
    scroll_lines_down();
  }

  if (s_line_lengths[0] < TX_TEXT_CAP) {
    s_lines[0][s_line_lengths[0]++] = value;
    s_lines[0][s_line_lengths[0]] = '\0';
  }
}

static char decode_code(const char *code) {
  for (size_t i = 0; i < sizeof(s_alphabet) / sizeof(s_alphabet[0]); ++i) {
    if (!strcmp(s_alphabet[i].code, code)) return s_alphabet[i].letter;
  }
  return '*';
}

static void finish_letter(void) {
  if (!s_code_length) return;

  append_text(decode_code(s_code));
  s_code_length = 0;
  s_code[0] = '\0';
  draw_message();
}

static void letter_timeout(void *context) {
  (void)context;
  s_letter_timer = NULL;
  finish_letter();
}

static void word_timeout(void *context) {
  (void)context;
  s_word_timer = NULL;
  finish_letter();

  if (s_line_lengths[0] &&
      s_lines[0][s_line_lengths[0] - 1] != ' ') {
    append_text(' ');
    draw_message();
  }
}

static void prepare_mark(void) {
  if (s_last_mark_end && s_code_length) {
    const uint32_t gap = now_ms() - s_last_mark_end;

    if (gap >= 3u * s_unit_ms) {
      finish_letter();
    }

    if (gap >= 7u * s_unit_ms &&
        s_line_lengths[0] &&
        s_lines[0][s_line_lengths[0] - 1] != ' ') {
      append_text(' ');
    }
  }

  cancel_timer(&s_letter_timer);
  cancel_timer(&s_word_timer);
}

static void schedule_decode(void) {
  s_last_mark_end = now_ms();

  cancel_timer(&s_letter_timer);
  cancel_timer(&s_word_timer);

  s_letter_timer = app_timer_register(3u * s_unit_ms, letter_timeout, NULL);
  s_word_timer = app_timer_register(7u * s_unit_ms, word_timeout, NULL);
}

static void record_symbol(char symbol) {
  if (s_code_length < TX_CODE_CAP) {
    s_code[s_code_length++] = symbol;
    s_code[s_code_length] = '\0';
  } else {
    s_code_length = TX_CODE_CAP;
  }

  schedule_decode();
}

static void key_down(uint32_t duration_ms) {
  const uint32_t tone_ms =
      duration_ms > TX_VIBE_MAX ? TX_VIBE_MAX : duration_ms;

  // Timed paddle elements should end naturally. Stopping the speaker just
  // before every new tone truncates the previous mark and makes short dits
  // sound clipped. Recover only if the previous tone is still draining.
  if (s_settings.tone_enabled) {
    if (speaker_get_status() != SpeakerStatusIdle) {
      speaker_stop();
    }
    if (!speaker_play_tone(s_settings.tone_hz, tone_ms, TONE_VOLUME,
                           SpeakerWaveformSquare)) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Tone failed at %u Hz",
              (unsigned)s_settings.tone_hz);
    }
  }

  if (s_settings.vibration_enabled) {
    s_vibration_duration = tone_ms;
    VibePattern vibe = {
      .durations = &s_vibration_duration,
      .num_segments = 1
    };
    vibes_enqueue_custom_pattern(vibe);
  }
}

// Finish a timed paddle element without cutting off the speaker's own
// duration envelope. A straight key still uses key_up() for immediate stop.
static void key_mark_complete(void) {
  vibes_cancel();
}

static void key_up(void) {
  speaker_stop();
  vibes_cancel();
}

static void begin_paddle(void);

static void gap_done(void *context) {
  (void)context;
  s_gap_timer = NULL;
  begin_paddle();
}

static void mark_done(void *context) {
  (void)context;
  s_mark_timer = NULL;

  if (!s_active || !s_paddle_mark) return;

  s_paddle_mark = false;
  key_mark_complete();
  keyer_anim_release();
  record_symbol(s_mark_symbol);

  s_gap_timer = app_timer_register(s_unit_ms, gap_done, NULL);
}

static void begin_paddle(void) {
  if (!s_active || s_settings.straight_key_mode ||
      s_straight_held || s_paddle_mark || s_gap_timer) return;

  const bool up = s_up_pending || s_up_held;
  const bool down = s_down_pending || s_down_held;

  if (!up && !down) return;

  // Simple squeeze keyer: when both are held, alternate elements.
  const bool choose_up = (up && down) ? s_last_paddle != 1 : up;

  if (choose_up) {
    s_up_pending = false;
  } else {
    s_down_pending = false;
  }

  s_last_paddle = choose_up ? 1 : -1;

  // Normal: UP=dit, DOWN=dah. Reversed swaps the physical paddles.
  const bool dit = choose_up != s_settings.reverse_paddles;
  const uint32_t duration = dit ? s_unit_ms : 3u * s_unit_ms;

  s_mark_symbol = dit ? '.' : '-';
  s_paddle_mark = true;

  prepare_mark();
  keyer_anim_press();
  key_down(duration);

  s_mark_timer = app_timer_register(duration, mark_done, NULL);
  if (!s_mark_timer) {
    s_paddle_mark = false;
    key_up();
    keyer_anim_release();
  }
}

static void straight_key_down(void) {
  if (!s_active || s_straight_held || s_paddle_mark || s_gap_timer) return;

  prepare_mark();
  s_straight_held = true;
  s_straight_press_start = now_ms();
  keyer_anim_press();
  // Keep tone/vibration active while held; key_up stops both on release.
  key_down(TX_VIBE_MAX);
}

static void straight_key_up(void) {
  if (!s_straight_held) return;

  const uint32_t held_ms = now_ms() - s_straight_press_start;
  s_straight_held = false;
  key_up();
  keyer_anim_release();

  // At two dit units or longer, decode the straight-key closure as a dah.
  record_symbol(held_ms >= 2u * s_unit_ms ? '-' : '.');
}

static void up_down(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;

  if (!s_active) return;
  if (s_settings.straight_key_mode) {
    if (s_settings.straight_key_uses_up) straight_key_down();
    return;
  }

  s_up_held = true;
  s_up_pending = true;
  begin_paddle();
}

static void up_up(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;

  if (s_settings.straight_key_mode) {
    if (s_settings.straight_key_uses_up) straight_key_up();
    return;
  }
  s_up_held = false;
}

static void down_down(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;

  if (!s_active) return;
  if (s_settings.straight_key_mode) {
    if (!s_settings.straight_key_uses_up) straight_key_down();
    return;
  }

  s_down_held = true;
  s_down_pending = true;
  begin_paddle();
}

static void down_up(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;

  if (s_settings.straight_key_mode) {
    if (!s_settings.straight_key_uses_up) straight_key_up();
    return;
  }
  s_down_held = false;
}

static void settings_menu_push(void);

static void select_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  settings_menu_push();
}

static void main_click_config(void *context) {
  (void)context;

  window_raw_click_subscribe(BUTTON_ID_UP, up_down, up_up, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, down_down, down_up, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

static void stop_keying(void) {
  s_active = false;

  cancel_timer(&s_mark_timer);
  cancel_timer(&s_gap_timer);
  cancel_timer(&s_letter_timer);
  cancel_timer(&s_word_timer);

  s_up_held = false;
  s_down_held = false;
  s_up_pending = false;
  s_down_pending = false;
  s_paddle_mark = false;
  s_straight_held = false;

  key_up();
  keyer_anim_reset();
}

static void rail_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  // The sole remaining control is the clipped SELECT/menu semicircle.
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx,
                       GPoint(bounds.size.w + 2, bounds.size.h / 2),
                       8);
}

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  const int16_t content_width = bounds.size.w;
  s_message_width = content_width - 6;

  window_set_background_color(window, APP_BLUE);

  // The 14 animation frames are normal Pebble bitmap resources. Only the
  // currently displayed frame is kept in memory.
  s_art_bitmap_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_background_color(s_art_bitmap_layer, GColorClear);
  bitmap_layer_set_compositing_mode(s_art_bitmap_layer, GCompOpSet);
  bitmap_layer_set_alignment(s_art_bitmap_layer, GAlignTopLeft);
  layer_add_child(root, bitmap_layer_get_layer(s_art_bitmap_layer));

  keyer_anim_reset();

  // Three explicit rows: new text stays on row 1. When it no longer fits,
  // the previous rows shift down and the next character starts at column 1.
  s_message_layer = text_layer_create(
      GRect(3, 128,
            content_width - 6,
            bounds.size.h - 130));
  text_layer_set_background_color(s_message_layer, GColorClear);
  text_layer_set_text_color(s_message_layer, GColorWhite);
  text_layer_set_font(
      s_message_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_message_layer, GTextAlignmentLeft);
  text_layer_set_overflow_mode(s_message_layer, GTextOverflowModeWordWrap);
  layer_add_child(root, text_layer_get_layer(s_message_layer));

  // Keep only the clipped SELECT semicircle at the screen edge. The text
  // layer uses the full width underneath it.
  s_rail_layer = layer_create(
      GRect(bounds.size.w - ACTION_BAR_WIDTH, 0,
            ACTION_BAR_WIDTH, bounds.size.h));
  layer_set_update_proc(s_rail_layer, rail_update_proc);
  layer_add_child(root, s_rail_layer);

  draw_message();
}

static void main_window_unload(Window *window) {
  (void)window;
  stop_keying();

  if (s_message_layer) {
    text_layer_destroy(s_message_layer);
    s_message_layer = NULL;
  }

  if (s_art_bitmap_layer) {
    bitmap_layer_destroy(s_art_bitmap_layer);
    s_art_bitmap_layer = NULL;
  }

  if (s_keyer_bitmap) {
    gbitmap_destroy(s_keyer_bitmap);
    s_keyer_bitmap = NULL;
  }

  if (s_rail_layer) {
    layer_destroy(s_rail_layer);
    s_rail_layer = NULL;
  }
}

static void main_window_appear(Window *window) {
  (void)window;
  refresh_timing();
  s_active = true;
  keyer_anim_reset();

  if (s_rail_layer) {
    layer_mark_dirty(s_rail_layer);
  }
}

static void main_window_disappear(Window *window) {
  (void)window;
  stop_keying();
}

// ---------------------------------------------------------------------------
// Speed editor
// ---------------------------------------------------------------------------

static void speed_in_stopped(Animation *animation,
                             bool finished,
                             void *context) {
  (void)finished;
  (void)context;
  animation_destroy(animation);
}

static void speed_out_stopped(Animation *animation,
                              bool finished,
                              void *context) {
  (void)finished;
  (void)context;

  animation_destroy(animation);

  text_layer_set_text(s_speed_text_layer, s_speed_next_text);

  Layer *text_layer = text_layer_get_layer(s_speed_text_layer);
  GRect final_frame = layer_get_frame(text_layer);
  GRect start_frame = final_frame;

  start_frame.origin.x += s_speed_anim_direction * SPEED_ANIM_DISTANCE;
  final_frame.origin.x = 0;

  layer_set_frame(text_layer, start_frame);

  PropertyAnimation *in_prop =
      property_animation_create_layer_frame(
          text_layer, &start_frame, &final_frame);

  Animation *in_anim = property_animation_get_animation(in_prop);
  animation_set_curve(in_anim, AnimationCurveEaseInOut);
  animation_set_duration(in_anim, SPEED_ANIM_DURATION);
  animation_set_handlers(in_anim, (AnimationHandlers) {
    .stopped = speed_in_stopped
  }, NULL);
  animation_schedule(in_anim);
}

static void animate_speed_change(int direction) {
  if (!s_speed_text_layer) return;

  snprintf(s_speed_next_text,
           sizeof(s_speed_next_text),
           "%u WPM",
           (unsigned)s_settings.wpm);

  s_speed_anim_direction = direction;

  Layer *text_layer = text_layer_get_layer(s_speed_text_layer);
  GRect start = layer_get_frame(text_layer);
  GRect finish = start;

  finish.origin.x -= direction * SPEED_ANIM_DISTANCE;

  PropertyAnimation *out_prop =
      property_animation_create_layer_frame(text_layer, &start, &finish);

  Animation *out_anim = property_animation_get_animation(out_prop);
  animation_set_curve(out_anim, AnimationCurveEaseInOut);
  animation_set_duration(out_anim, SPEED_ANIM_DURATION);
  animation_set_handlers(out_anim, (AnimationHandlers) {
    .stopped = speed_out_stopped
  }, NULL);
  animation_schedule(out_anim);
}

static void speed_up_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;

  if (s_settings.wpm >= MAX_WPM) return;

  s_settings.wpm++;
  settings_save();
  refresh_timing();
  animate_speed_change(1);
}

static void speed_down_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;

  if (s_settings.wpm <= MIN_WPM) return;

  s_settings.wpm--;
  settings_save();
  refresh_timing();
  animate_speed_change(-1);
}

static void speed_click_config(void *context) {
  (void)context;
  window_single_repeating_click_subscribe(
      BUTTON_ID_UP, 140, speed_up_click);
  window_single_repeating_click_subscribe(
      BUTTON_ID_DOWN, 140, speed_down_click);
}

static void speed_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, APP_BLUE);

  snprintf(s_speed_text,
           sizeof(s_speed_text),
           "%u WPM",
           (unsigned)s_settings.wpm);

  s_speed_text_layer = text_layer_create(
      GRect(0, bounds.size.h / 2 - 28, bounds.size.w, 56));
  text_layer_set_background_color(s_speed_text_layer, GColorClear);
  text_layer_set_text_color(s_speed_text_layer, GColorWhite);
  text_layer_set_font(
      s_speed_text_layer,
      fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(
      s_speed_text_layer, GTextAlignmentCenter);
  text_layer_set_text(s_speed_text_layer, s_speed_text);
  layer_add_child(root, text_layer_get_layer(s_speed_text_layer));
}

static void speed_window_unload(Window *window) {
  text_layer_destroy(s_speed_text_layer);
  s_speed_text_layer = NULL;

  window_destroy(window);
  s_speed_window = NULL;

  if (s_menu_layer) {
    menu_layer_reload_data(s_menu_layer);
  }
}

static void speed_window_push(void) {
  if (s_speed_window) return;

  s_speed_window = window_create();
  window_set_click_config_provider(s_speed_window, speed_click_config);
  window_set_window_handlers(s_speed_window, (WindowHandlers) {
    .load = speed_window_load,
    .unload = speed_window_unload
  });
  window_stack_push(s_speed_window, true);
}

// ---------------------------------------------------------------------------
// Tone-frequency editor
// ---------------------------------------------------------------------------

static void tone_refresh_text(void) {
  snprintf(s_tone_text, sizeof(s_tone_text), "%u Hz",
           (unsigned)s_settings.tone_hz);
  if (s_tone_text_layer) text_layer_set_text(s_tone_text_layer, s_tone_text);
  if (s_menu_layer) menu_layer_reload_data(s_menu_layer);
}

static void tone_adjust(int direction) {
  int value = (int)s_settings.tone_hz + direction * TONE_HZ_STEP;
  if (value < MIN_TONE_HZ) value = MIN_TONE_HZ;
  if (value > MAX_TONE_HZ) value = MAX_TONE_HZ;
  if (value == s_settings.tone_hz) return;

  s_settings.tone_hz = (uint16_t)value;
  settings_save();
  tone_refresh_text();
}

static void tone_up_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  tone_adjust(1);
}

static void tone_down_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  tone_adjust(-1);
}

static void tone_back_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  window_stack_pop(true);
}

static void tone_click_config(void *context) {
  (void)context;
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 140, tone_up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 140, tone_down_click);
  window_single_click_subscribe(BUTTON_ID_BACK, tone_back_click);
}

static void tone_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, APP_BLUE);

  s_tone_text_layer = text_layer_create(
      GRect(0, bounds.size.h / 2 - 28, bounds.size.w, 56));
  text_layer_set_background_color(s_tone_text_layer, GColorClear);
  text_layer_set_text_color(s_tone_text_layer, GColorWhite);
  text_layer_set_font(s_tone_text_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_tone_text_layer, GTextAlignmentCenter);
  tone_refresh_text();
  layer_add_child(root, text_layer_get_layer(s_tone_text_layer));
}

static void tone_window_unload(Window *window) {
  (void)window;
  if (s_tone_text_layer) {
    text_layer_destroy(s_tone_text_layer);
    s_tone_text_layer = NULL;
  }
  window_destroy(s_tone_window);
  s_tone_window = NULL;
  if (s_menu_layer) menu_layer_reload_data(s_menu_layer);
}

static void tone_window_push(void) {
  if (s_tone_window) return;
  s_tone_window = window_create();
  window_set_click_config_provider(s_tone_window, tone_click_config);
  window_set_window_handlers(s_tone_window, (WindowHandlers) {
    .load = tone_window_load,
    .unload = tone_window_unload
  });
  window_stack_push(s_tone_window, true);
}

// ---------------------------------------------------------------------------
// Vibration battery warning
// ---------------------------------------------------------------------------

static void vibration_warning_enable(ClickRecognizerRef recognizer,
                                     void *context) {
  (void)recognizer; (void)context;
  s_settings.vibration_enabled = true;
  settings_save();
  window_stack_pop(true);
}

static void vibration_warning_cancel(ClickRecognizerRef recognizer,
                                     void *context) {
  (void)recognizer; (void)context;
  window_stack_pop(true);
}

static void vibration_warning_click_config(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_SELECT, vibration_warning_enable);
  window_single_click_subscribe(BUTTON_ID_BACK, vibration_warning_cancel);
}

static void vibration_warning_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, GColorWhite);

  s_vibration_warning_text_layer = text_layer_create(
      GRect(10, 30, bounds.size.w - 20, bounds.size.h - 60));
  text_layer_set_background_color(s_vibration_warning_text_layer, GColorClear);
  text_layer_set_text_color(s_vibration_warning_text_layer, GColorBlack);
  text_layer_set_font(s_vibration_warning_text_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_vibration_warning_text_layer,
                                GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_vibration_warning_text_layer,
                               GTextOverflowModeWordWrap);
  text_layer_set_text(s_vibration_warning_text_layer,
                      "Vibration uses extra battery.\n\nSELECT: Enable\nBACK: Cancel");
  layer_add_child(root,
                  text_layer_get_layer(s_vibration_warning_text_layer));
}

static void vibration_warning_unload(Window *window) {
  (void)window;
  if (s_vibration_warning_text_layer) {
    text_layer_destroy(s_vibration_warning_text_layer);
    s_vibration_warning_text_layer = NULL;
  }
  window_destroy(s_vibration_warning_window);
  s_vibration_warning_window = NULL;
  if (s_menu_layer) menu_layer_reload_data(s_menu_layer);
}

static void vibration_warning_push(void) {
  if (s_vibration_warning_window) return;
  s_vibration_warning_window = window_create();
  window_set_click_config_provider(s_vibration_warning_window,
                                   vibration_warning_click_config);
  window_set_window_handlers(s_vibration_warning_window, (WindowHandlers) {
    .load = vibration_warning_load,
    .unload = vibration_warning_unload
  });
  window_stack_push(s_vibration_warning_window, true);
}

// ---------------------------------------------------------------------------
// Paddle choice dialog
// ---------------------------------------------------------------------------

static void paddle_choose_normal(ClickRecognizerRef recognizer,
                                 void *context) {
  (void)recognizer;
  (void)context;

  s_settings.reverse_paddles = false;
  settings_save();
  window_stack_pop(true);
}

static void paddle_choose_reversed(ClickRecognizerRef recognizer,
                                   void *context) {
  (void)recognizer;
  (void)context;

  s_settings.reverse_paddles = true;
  settings_save();
  window_stack_pop(true);
}

static void paddle_choice_click_config(void *context) {
  (void)context;
  window_single_click_subscribe(
      BUTTON_ID_UP, paddle_choose_normal);
  window_single_click_subscribe(
      BUTTON_ID_DOWN, paddle_choose_reversed);
}

static void paddle_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorWhite);

  s_paddle_prompt_layer = text_layer_create(
      GRect(10, 58,
            bounds.size.w - ACTION_BAR_WIDTH - 20,
            112));
  text_layer_set_background_color(
      s_paddle_prompt_layer, GColorClear);
  text_layer_set_text_color(
      s_paddle_prompt_layer, GColorBlack);
  text_layer_set_font(
      s_paddle_prompt_layer,
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(
      s_paddle_prompt_layer, GTextAlignmentCenter);
  text_layer_set_text(
      s_paddle_prompt_layer,
      "What should\nUP send?");
  layer_add_child(root,
                  text_layer_get_layer(s_paddle_prompt_layer));

  s_dit_icon =
      gbitmap_create_with_resource(RESOURCE_ID_ICON_DIT);
  s_dah_icon =
      gbitmap_create_with_resource(RESOURCE_ID_ICON_DAH);

  s_paddle_action_bar = action_bar_layer_create();
  action_bar_layer_set_background_color(
      s_paddle_action_bar, GColorWhite);
  action_bar_layer_set_icon(
      s_paddle_action_bar, BUTTON_ID_UP, s_dit_icon);
  action_bar_layer_set_icon(
      s_paddle_action_bar, BUTTON_ID_DOWN, s_dah_icon);
  action_bar_layer_set_icon_press_animation(
      s_paddle_action_bar,
      BUTTON_ID_UP,
      ActionBarLayerIconPressAnimationMoveLeft);
  action_bar_layer_set_icon_press_animation(
      s_paddle_action_bar,
      BUTTON_ID_DOWN,
      ActionBarLayerIconPressAnimationMoveLeft);
  action_bar_layer_set_click_config_provider(
      s_paddle_action_bar,
      paddle_choice_click_config);
  action_bar_layer_add_to_window(
      s_paddle_action_bar, window);
}

static void paddle_window_unload(Window *window) {
  text_layer_destroy(s_paddle_prompt_layer);
  s_paddle_prompt_layer = NULL;

  if (s_paddle_action_bar) {
    action_bar_layer_remove_from_window(s_paddle_action_bar);
    action_bar_layer_destroy(s_paddle_action_bar);
    s_paddle_action_bar = NULL;
  }

  if (s_dit_icon) {
    gbitmap_destroy(s_dit_icon);
    s_dit_icon = NULL;
  }

  if (s_dah_icon) {
    gbitmap_destroy(s_dah_icon);
    s_dah_icon = NULL;
  }

  window_destroy(window);
  s_paddle_window = NULL;

  if (s_menu_layer) {
    menu_layer_reload_data(s_menu_layer);
  }
}

static void paddle_window_push(void) {
  if (s_paddle_window) return;

  s_paddle_window = window_create();
  window_set_window_handlers(s_paddle_window, (WindowHandlers) {
    .load = paddle_window_load,
    .unload = paddle_window_unload
  });
  window_stack_push(s_paddle_window, true);
}

// ---------------------------------------------------------------------------
// Settings MenuLayer
// ---------------------------------------------------------------------------

static uint16_t menu_get_num_rows(MenuLayer *menu_layer,
                                  uint16_t section_index,
                                  void *context) {
  (void)menu_layer;
  (void)section_index;
  (void)context;
  return 7;
}

static void menu_draw_row(GContext *ctx,
                          const Layer *cell_layer,
                          MenuIndex *cell_index,
                          void *context) {
  (void)context;

  char subtitle[32];
  const char *title = "";

  switch (cell_index->row) {
    case 0:
      title = "Speed";
      snprintf(subtitle, sizeof(subtitle), "%u WPM",
               (unsigned)s_settings.wpm);
      break;
    case 1:
      title = "Keying mode";
      snprintf(subtitle, sizeof(subtitle), "%s",
               s_settings.straight_key_mode ? "Straight key" : "Paddle");
      break;
    case 2:
      title = "Straight key";
      snprintf(subtitle, sizeof(subtitle), "%s button",
               s_settings.straight_key_uses_up ? "Top" : "Bottom");
      break;
    case 3:
      title = "Tone frequency";
      snprintf(subtitle, sizeof(subtitle), "%u Hz",
               (unsigned)s_settings.tone_hz);
      break;
    case 4:
      title = "Tone";
      snprintf(subtitle, sizeof(subtitle), "%s",
               s_settings.tone_enabled ? "On" : "Off");
      break;
    case 5:
      title = "Vibration";
      snprintf(subtitle, sizeof(subtitle), "%s",
               s_settings.vibration_enabled ? "On" : "Off");
      break;
    default:
      title = "Paddles";
      snprintf(subtitle, sizeof(subtitle), "%s",
               s_settings.reverse_paddles
                   ? "UP dah / DOWN dit"
                   : "UP dit / DOWN dah");
      break;
  }

  menu_cell_basic_draw(ctx, cell_layer, title, subtitle, NULL);
}

static void menu_select(MenuLayer *menu_layer,
                        MenuIndex *cell_index,
                        void *context) {
  (void)menu_layer;
  (void)context;

  switch (cell_index->row) {
    case 0:
      speed_window_push();
      break;
    case 1:
      s_settings.straight_key_mode = !s_settings.straight_key_mode;
      settings_save();
      menu_layer_reload_data(s_menu_layer);
      break;
    case 2:
      s_settings.straight_key_uses_up = !s_settings.straight_key_uses_up;
      settings_save();
      menu_layer_reload_data(s_menu_layer);
      break;
    case 3:
      tone_window_push();
      break;
    case 4:
      s_settings.tone_enabled = !s_settings.tone_enabled;
      settings_save();
      menu_layer_reload_data(s_menu_layer);
      break;
    case 5:
      if (s_settings.vibration_enabled) {
        s_settings.vibration_enabled = false;
        settings_save();
        menu_layer_reload_data(s_menu_layer);
      } else {
        vibration_warning_push();
      }
      break;
    default:
      paddle_window_push();
      break;
  }
}

static void menu_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_menu_callbacks = (MenuLayerCallbacks) {
    .get_num_rows = menu_get_num_rows,
    .draw_row = menu_draw_row,
    .select_click = menu_select
  };

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(
      s_menu_layer, NULL, s_menu_callbacks);
  menu_layer_set_normal_colors(
      s_menu_layer, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(
      s_menu_layer, APP_BLUE, GColorWhite);
  menu_layer_set_click_config_onto_window(
      s_menu_layer, window);
  menu_layer_pad_bottom_enable(s_menu_layer, true);

  layer_add_child(root, menu_layer_get_layer(s_menu_layer));
}

static void menu_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  s_menu_layer = NULL;

  window_destroy(window);
  s_menu_window = NULL;

  if (s_rail_layer) {
    layer_mark_dirty(s_rail_layer);
  }
}

static void settings_menu_push(void) {
  if (s_menu_window) return;

  s_menu_window = window_create();
  window_set_window_handlers(s_menu_window, (WindowHandlers) {
    .load = menu_window_load,
    .unload = menu_window_unload
  });
  window_stack_push(s_menu_window, true);
}

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------

static void init(void) {
  settings_load();

  memset(s_lines, 0, sizeof(s_lines));
  memset(s_line_lengths, 0, sizeof(s_line_lengths));
  s_display_text[0] = '\0';
  s_code[0] = '\0';
  s_code_length = 0;
  s_last_mark_end = 0;

  s_main_window = window_create();
  window_set_click_config_provider(
      s_main_window, main_click_config);
  window_set_window_handlers(
      s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload,
        .appear = main_window_appear,
        .disappear = main_window_disappear
      });

  window_stack_push(s_main_window, true);
}

static void deinit(void) {
  stop_keying();

  if (s_main_window) {
    window_destroy(s_main_window);
    s_main_window = NULL;
  }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
