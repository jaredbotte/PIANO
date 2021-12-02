#ifndef __LEDS_H__
#define __LEDS_H__
#include <stdint.h>

//LED Strip Colors
#define BRIGHTNESS 24.0 / 255.0
#define OFF (Color) {.red = 0, .green = 0, .blue = 0}
#define GREEN (Color) {.red = 0, .green = 255 * BRIGHTNESS, .blue = 0}
#define BLUE (Color) {.red = 0, .green = 0, .blue = 255 * BRIGHTNESS}
#define ORANGE (Color) {.red = 255 * BRIGHTNESS, .green = 255 * BRIGHTNESS, .blue = 0}
#define RED (Color) {.red = 255 * BRIGHTNESS, .green = 0, .blue = 0}
#define MAGENTA (Color) {.red = 255 * BRIGHTNESS, .green = 0, .blue = 255 * BRIGHTNESS}
#define CYAN (Color) {.red = 0, .green = 255 * BRIGHTNESS, .blue = 255 * BRIGHTNESS}
#define WHITE (Color) {.red = 255 * BRIGHTNESS, .green = 255 * BRIGHTNESS, .blue = 255 * BRIGHTNESS}
#define GOLD (Color) {.red = 184 * BRIGHTNESS, .green = 134 * BRIGHTNESS, .blue = 11 * BRIGHTNESS}
#define LEARN_COLOR BLUE
#define INCORRECT_COLOR RED
#define CORRECT_COLOR GREEN
#define ERROR_COLOR GOLD

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
    bool systemLit;
    bool userLit;
    int hitStreak;
} Key;

//Init functions
void setup_led_pwm_dma();
void start_timer();
void initIndication();
void setup_key_array(int num_keys);
void initialize_led_strip(int ledNum, int pinNum, int keyNum);

//Low Level LED Control
void update_led_strip();
void ledIndicate(int);
void set_led(int led_num, Color color);
bool areSameColor(Color a, Color b);
Color get_led_color(int led_num);
void fill_color(Color color);
void fill_test();



//High Level LED/Key Control
void set_key(uint8_t key_num, bool keyOn, bool isSystemLit, int velocity, Color color);
void set_key_velocity(uint8_t key_num, bool keyOn, int velocity);
void set_key_play(uint8_t key_num, bool keyOn, int velocity);
void set_key_learn(uint8_t key_num, bool keyOn, int velocity);
Color get_key_color(uint8_t key_num);
bool isLearnSetFinished();
void led_connect_animation();

#endif // __LEDS_H__