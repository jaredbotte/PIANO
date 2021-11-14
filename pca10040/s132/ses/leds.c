#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "leds.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

int num_leds, led_pin, num_keys;
int keysPressed, incorrectKeys;
uint16_t* buffer;
Key* key_array;
static bool advertisingBLE = false;
static int userKeys[88] = {0};


void PWM0_IRQHandler(){
   NRF_PWM0 -> EVENTS_SEQEND[0] = 0;
   while(NRF_PWM0 -> EVENTS_SEQEND[0]);
}


void reset_ltp(){
    keysPressed = 0;
    incorrectKeys = 0;
}


void addIncorrect(){
    incorrectKeys++;
}


void start_timer(void)
{		
  NRF_TIMER3->MODE = TIMER_MODE_MODE_Timer;
  NRF_TIMER3->TASKS_CLEAR = 1;
  NRF_TIMER3->PRESCALER = 3; //TODO play with this to see if we can get note alternation test working properly
  NRF_TIMER3->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
  NRF_TIMER3->CC[0] = 10000;
  NRF_TIMER3->INTENSET = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
  NVIC_EnableIRQ(TIMER3_IRQn);

  NRF_TIMER4->MODE = TIMER_MODE_MODE_Timer;
  NRF_TIMER4->TASKS_CLEAR = 1;
  NRF_TIMER4->PRESCALER = 7; //TODO Ensure this makes a reasonable LED blink speed
  NRF_TIMER4->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
  NRF_TIMER4->CC[0] = 10000;
  NRF_TIMER4->INTENSET = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
  NVIC_EnableIRQ(TIMER4_IRQn);

  NRF_TIMER3->TASKS_START = 1;
  NRF_TIMER4->TASKS_START = 1;
}


void TIMER4_IRQHandler(void)
{
  NRF_TIMER4->EVENTS_COMPARE[0] = 0;	
  if(advertisingBLE) {
    nrf_gpio_pin_toggle(LED_BLUE);
  }
}


static void setup_led_pwm_dma(){
    // GPIO setup
    NRF_GPIO -> DIRSET = (1 << led_pin);  // set pin to output
    NRF_GPIO -> OUTCLR = (1 << led_pin);  // set pin to pull low when not in use

    // PWM setup
    NRF_PWM0 -> PSEL.OUT[0] = led_pin;  // Set PWM0 Channel 0 to output on led_pin
    NRF_PWM0 -> ENABLE = PWM_ENABLE_ENABLE_Enabled << PWM_ENABLE_ENABLE_Pos; // Enable PWM0
    NRF_PWM0 -> MODE = PWM_MODE_UPDOWN_Up << PWM_MODE_UPDOWN_Pos; // Set PWM mode to upcounter
    NRF_PWM0 -> PRESCALER = 0;  // Use the full 16MHz peripheral clock
    NRF_PWM0 -> COUNTERTOP = 20;  // 1.25 us period
    NRF_PWM0 -> LOOP = 0;  // Don't loop
    NRF_PWM0 -> DECODER = PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos | PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos;
    
    // SEQ[0] Will be our buffer
    NRF_PWM0 -> SEQ[0].PTR = (uint32_t) buffer;  // The pointer to bufferA
    NRF_PWM0 -> SEQ[0].CNT = (24 * num_leds) + 40;  // Number of elements in the buffer
    NRF_PWM0 -> SEQ[0].REFRESH = 0;  // Send each element only once
    NRF_PWM0 -> SEQ[0].ENDDELAY = 0;  // Don't delay after finishing

    // Interrupt setup
    NVIC -> ISER[0] = 1 << PWM0_IRQn;
    NRF_PWM0 -> INTENSET = PWM_INTENSET_SEQEND0_Enabled << PWM_INTENSET_SEQEND0_Pos;
}


static void setup_key_array(int num_keys){
    int key_led = 0;
    int width = 2; // For now we'll make each key two LEDs wide. Actually pretty darn close!
    for(int k = 0; k < num_keys; k++){
        key_array[k] = (Key) {.starting_led = key_led, .num_led = width, .systemLit = false, .userLit = false, .color = OFF};
        key_led += width;
    }
}


void updateKeys() {
    for(int i = 0; i < num_keys; i++) {
        Key current_key = key_array[i];
        for(int j = current_key.starting_led; j < current_key.starting_led + current_key.num_led; j++){
            set_led(j, current_key.color);
        }
    }
    update_led_strip();
}


void update_led_strip(){
    NRF_PWM0 -> TASKS_SEQSTART[0] = 1;
}


void fill_color(Color color){
    for(int n = 0; n < num_leds; n++){
        set_led(n, color);
    }
}


bool isLearnSetFinished(){
    for(int i = 0; i < num_keys; i++) {
        Key current_key = key_array[i];
        bool wtfsegger = (current_key.systemLit && !current_key.userLit);
        if((current_key.systemLit && !current_key.userLit) || (!current_key.systemLit && current_key.userLit)) {
            return false;
        }
    }
    return true;
}


void led_connect_animation(){ //TODO
    // Will need to figure out how to delay something so we can make an animation.
    //fill_color(RED);
}


void set_led(int led_num, Color color){
    uint32_t col = (color.green << 16) | (color.red << 8) | color.blue;
    int led_ind = led_num * 24;
    for(int i = 0; i < 24; i++){
        buffer[led_ind + i] = (((col & (1 << (23 - i))) >> (23 - i)) * 6 + 6) | 0x8000;
    }
}


Color get_led_color(int led_num){
    uint32_t col = 0;
    int led_ind = led_num *24;
    for(int i = 0; i < 24; i++){
        uint16_t val = buffer[led_ind + i];
        if(val == 0x8006){
            val = 0;
        } else {
            val = 1;
        }
        col <<= 1;
        col |= val;
    }
    return (Color) {.green = (col >> 16) & 0xff, .red = (col >> 8) & 0Xff, .blue = col & 0xff};
}


Color get_key_color(uint8_t key_num){
    return get_led_color(key_array[key_num].starting_led);
}


void set_key(uint8_t key_num, bool keyOn, bool isSystemLit, int velocity, Color color){
    if(key_num >= 88){
        return;
    }
    Key* current_key = &key_array[key_num];
    if(!keyOn || velocity == 0){
        current_key->color = OFF;
        if(isSystemLit)
            current_key->systemLit = false;
        else  
            current_key->userLit = false;
    }
    else {
        current_key->color = color;
        if(isSystemLit)
            current_key->systemLit = true;
        else  
            current_key->userLit = true;
    }
}


void set_key_velocity(uint8_t key_num, bool keyOn, int velocity){
    Color color = (Color) {.red = 0, .green = 63, .blue = 0};
    if(!keyOn) {
        color = OFF;
    } 
    else if (velocity > 127.0 * 0.6) {
        color = RED;
    } 
    else if (velocity > 127.0 * 0.3) {
        color = ORANGE;
    } 
    else {
        color = GREEN;
    }
    set_key(key_num, keyOn, true, velocity, color);
}


bool areSameColor(Color a, Color b){
    return (a.red == b.red && a.green == b.green && a.blue == b.blue);
}  


void set_key_learn(uint8_t key_num, bool keyOn, int velocity){
    Key* current_key = &key_array[key_num];
    if(keyOn) {
        if(current_key->systemLit) {
            set_key(key_num, true, false, velocity, CORRECT_COLOR);
        }
        else {
            set_key(key_num, true, false, velocity, INCORRECT_COLOR);
        }
    } 
    else {
        if(current_key->systemLit) {
            current_key->userLit = false;
            set_key(key_num, true, true, velocity, LEARN_COLOR);
        }
        else {
            set_key(key_num, false, false, velocity, OFF);
        }
    }
}


void set_key_play(int key_num, int stat){ //TODO 
    // NOTE: We can make this do something if we want.
    // This function is called in PA Mode when a key is pressed.
}


void fill_test(){
  Color red = (Color) {.red = 0, .green = 32, .blue = 0};
  Color blue = (Color) {.red = 0, .green = 0, .blue = 32};
  for(int n = 0; n < num_leds; n++){
     if (n % 2 == 0) {
      set_led(n, red);
    } else {
      set_led(n, blue);
    }
  }
}


void initialize_led_strip(int ledNum, int pinNum, int keyNum){
    num_keys = keyNum;
    num_leds = ledNum;
    led_pin = pinNum;
    buffer = malloc(sizeof(*buffer) * num_leds * 24 + sizeof(*buffer) * 40);
    if (buffer == NULL){
        printf("Memory Limit Reached :(");
    }
    key_array = malloc(sizeof(*key_array) * num_keys);
    setup_key_array(num_keys);
    for(int i = num_leds * 24; i < num_leds * 24 + 40; i++){
        buffer[i] = 0x8000;
    }
    setup_led_pwm_dma();
    fill_color((Color) {.red=0, .green=0, .blue=0});
    start_timer();

    keysPressed = 0; // TODO: Set to zero on LTP MODE Entry. Make sure it's never negative.
    incorrectKeys = 0;
    //fill_test();
}

//Inits on-board LED's
void initIndication() {
  nrf_gpio_cfg_output(LED_BLUE);
  nrf_gpio_cfg_output(LED_GREEN);
  ledIndicate(-1);
  ledIndicate(LED_POWER);
}


//Controls on-board indication LED's
void ledIndicate(int action) {
  advertisingBLE = false;
  switch(action) {
    case LED_POWER:
      nrf_gpio_pin_set(LED_GREEN);
      break;
    case LED_IDLE:
      nrf_gpio_pin_clear(LED_BLUE);
      break;
    case LED_ADVERTISING:
      advertisingBLE = true;
      break;
    case LED_CONNECTED:
      nrf_gpio_pin_set(LED_BLUE);
      break;
    default:
      nrf_gpio_pin_clear(LED_GREEN);
      nrf_gpio_pin_clear(LED_BLUE);
  }
}
