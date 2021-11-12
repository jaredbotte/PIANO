#ifndef __LEDS_H__
#define __LEDS_H__
#include <stdint.h>

#define BRIGHTNESS 24.0 / 255.0
#define OFF (Color) {.red = 0, .green = 0, .blue = 0}
#define GREEN (Color) {.red = 0, .green = 255 * BRIGHTNESS, .blue = 0}
#define BLUE (Color) {.red = 0, 0, .blue = 255 * BRIGHTNESS}
#define ORANGE (Color) {.red = 255 * BRIGHTNESS, .green = 255 * BRIGHTNESS, .blue = 0}
#define RED (Color) {.red = 255 * BRIGHTNESS, .green = 0, .blue = 0}
#define MAGENTA (Color) {.red = 255 * BRIGHTNESS, .green = 0, .blue = 255 * BRIGHTNESS}
#define CYAN (Color) {.red = 0, .green = 255 * BRIGHTNESS, .blue = 255 * BRIGHTNESS}
#define WHITE (Color) {.red = 255 * BRIGHTNESS, .green = 255 * BRIGHTNESS, .blue = 255 * BRIGHTNESS}
#define GOLD (Color) {.red = 184 * BRIGHTNESS, .green = 134 * BRIGHTNESS, .blue = 11 * BRIGHTNESS}

//Board Indications
#define LED_BLUE 6
#define LED_GREEN 7
#define LED_POWER 0
#define LED_IDLE 1
#define LED_ADVERTISING 2
#define LED_CONNECTED 3

typedef struct _Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Color;

typedef struct _Key {
    int starting_led;
    int num_led;
} Key;

// Static setup/critical functions
static void setup_led_pwm_dma();
static void setup_led_refresh(int rate_hz);
static void setup_key_array(int num_keys);
static void fill_buffer(uint16_t* buffer);
static void fill_buffer_color(uint16_t* buffer, uint32_t curr_col);
static void fill_buffer_reset(uint16_t* buffer);

// Interfacing functions
void initialize_led_strip(int num, int pin);
void update_led_strip();
void fill_color(Color color);
void fill_test();
void set_led(int led_num, Color color);
Color get_key_color(int key_num);
void set_key(int key_num, int stat, Color color);
void set_key_velocity(int key_num, int stat, int velocity);
Color get_led_color(int led_num);
bool areSameColor(Color a, Color b);
bool isNoteFinished(int keysToPress);
void reset_ltp();
void led_connect_animation();

//Board Indication
void initIndication();
void ledIndicate(int);

#endif // __LEDS_H__