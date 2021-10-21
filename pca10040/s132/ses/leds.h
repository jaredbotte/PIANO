#ifndef __LEDS_H__
#define __LEDS_H__
#include <stdint.h>

#define BRIGHTNESS 24.0 / 255.0
#define OFF (Color) {.red = 0, .green = 0, .blue = 0}
#define GREEN (Color) {.red = 0, .green = 255 * BRIGHTNESS, .blue = 0}
#define ORANGE (Color) {.red = 255 * BRIGHTNESS, .green = 255 * BRIGHTNESS, .blue = 0}
#define RED (Color) {.red = 255 * BRIGHTNESS, .green = 0, .blue = 0}

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
void initialize_led_strip(int num, int pin, int num_keys);
void update_led_strip();
void fill_color(Color color);
void fill_test();
void set_led(int led_num, Color color);
void set_key(int key_num, int stat, Color color);
void set_key_velocity(int key_num, int stat, int velocity);

#endif // __LEDS_H__