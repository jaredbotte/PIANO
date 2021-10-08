#ifndef __LEDS_H__
#define __LEDS_H__
#include <stdint.h>

typedef struct _Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Color;

// Static setup/critical functions
static void setup_led_pwm_dma();
static void fill_buffer(uint16_t* buffer);
static void fill_buffer_color(uint16_t* buffer, uint32_t curr_col);
static void fill_buffer_reset(uint16_t* buffer);

// Interfacing functions
void initialize_led_strip(int num, int pin);
void update_led_strip();
void fill_color(Color color);
void set_led(int led_num, Color color);

#endif // __LEDS_H__