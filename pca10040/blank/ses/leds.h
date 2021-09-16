#ifndef __LEDS_H__
#define __LEDS_H__
#include <stdint.h>

typedef struct _Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Color;

// Static setup functions
static void setup_led_pwm_dma();

// Statoc buffer related functions. We'll make sure these are protected.
static void fill_buffer1(Color color);
static void fill_buffer2(Color color);

// Interfacing functions
void initialize_led_strip(int num, int pin);
void update_led_strip();
void fill_duty_cycle_counts();
void fill_color(Color color);

#endif // __LEDS_H__