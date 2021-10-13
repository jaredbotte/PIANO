#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "leds.h"
#include "nrf.h"
#include "nrf_nvic.h"

int num_leds;
int led_pin;
int update_finished;
int current_led;
uint32_t* colors;
uint16_t* bufferA;
uint16_t* bufferB;
Key* key_array;

void PWM0_IRQHandler(){
    // This handles the DMA transfer complete interrupts.
    if(NRF_PWM0 -> EVENTS_SEQEND[0]){ // Buffer A has completed transfer. We need to fill it now.
        NRF_PWM0 -> TASKS_SEQSTART[1] = 1; // First, let's start transferring Buffer B
        NRF_PWM0 -> EVENTS_SEQEND[0] = 0; // Clear the interrupt for Buffer A
        fill_buffer(bufferA);
    } else { // Buffer B has completed transfer. We need to fill it now.
        NRF_PWM0 -> TASKS_SEQSTART[0] = 1; // Start transferring Buffer A
        NRF_PWM0 -> EVENTS_SEQEND[1] = 0; // Clear interrupt for Buffer B
        fill_buffer(bufferB);
    }
    current_led += 1;
}

// LED strip refresh code here

static void setup_led_refresh(int rate_hz){
  // This function will update the LEDs at the rate specified by rate_hz

  NRF_TIMER3 -> TASKS_STOP = 1;
  NRF_TIMER3 -> MODE = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
  NRF_TIMER3 -> BITMODE = TIMER_BITMODE_BITMODE_24Bit << TIMER_BITMODE_BITMODE_Pos;
  NRF_TIMER3 -> PRESCALER = 5; //f_timer = 16,000,000 / 2 ^ 5 = 500,000
  NRF_TIMER3 -> TASKS_CLEAR = 1;
  NRF_TIMER3 -> CC[0] = 500000 / (rate_hz);

  NRF_TIMER3 -> INTENSET = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;
  NRF_TIMER3 -> SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;
  NVIC -> ISER[0] = 1 << TIMER3_IRQn;
  NRF_TIMER3 -> TASKS_START = 1;
}

void TIMER3_IRQHandler(){
  // refresh the strip
  update_led_strip();
}


static void fill_buffer(uint16_t* buffer){
    if(current_led >= num_leds){
        if(current_led >= num_leds + 40) {
            update_finished = 1; // We've finished sending our data
            // TODO: Either shut down PWM or make sure counter doesn't overflow.
            current_led -= 1;
        }
        fill_buffer_reset(buffer);
    } else {
        uint32_t curr_col = colors[current_led];
        fill_buffer_color(buffer, curr_col);
    }
}

static void fill_buffer_color(uint16_t* buffer, uint32_t color){
    for (int i = 0; i < 24; i++){
        uint16_t val = (color & 0x1) * 6 + 6;
        val |= 0x8000;
        buffer[23 - i] = val;
        color = color >> 1;
    }
}

static void fill_buffer_reset(uint16_t* buffer){
    for(int i = 0; i < 24; i++){
        buffer[i] = 0x8000;
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
    
    // SEQ[0] Will be one of our buffers. Contains one LEDs worth of bytes
    NRF_PWM0 -> SEQ[0].PTR = (uint32_t) bufferA;  // The pointer to bufferA
    NRF_PWM0 -> SEQ[0].CNT = 24;  // Number of elements in the buffer
    NRF_PWM0 -> SEQ[0].REFRESH = 0;  // Send each element only once
    NRF_PWM0 -> SEQ[0].ENDDELAY = 0;  // Don't delay after finishing

    // SEQ[1] Will be the other buffer. Contains one LEDs worth of bytes
    NRF_PWM0 -> SEQ[1].PTR = (uint32_t) bufferB; // The pointer to bufferB
    NRF_PWM0 -> SEQ[1].CNT = 24;  // Number of elements in the buffer
    NRF_PWM0 -> SEQ[1].REFRESH = 0; // Send each elemnt only once
    NRF_PWM0 -> SEQ[1].ENDDELAY = 0; // Don't delay after finishing

    // Interrupt setup
    NVIC -> ISER[0] = 1 << PWM0_IRQn;
    NRF_PWM0 -> INTENSET = PWM_INTENSET_SEQEND0_Enabled << PWM_INTENSET_SEQEND0_Pos;
    NRF_PWM0 -> INTENSET = PWM_INTENSET_SEQEND1_Enabled << PWM_INTENSET_SEQEND1_Pos;
}

static void setup_key_array(int num_keys){
    int key_led = 0;
    int width = 2; // For now we'll make each key two LEDs wide. 
    for(int k = 0; k < num_keys; k++){
        key_array[k] = (Key) {.starting_led = key_led, .num_led = width};
        key_led += width;
    }
}

void update_led_strip(){
    while(update_finished == 0){
        __asm__("nop");
    }
    current_led = 0;
    update_finished = 0;
}

void fill_color(Color color){
    for(int n = 0; n < num_leds; n++){
        set_led(n, color);
    }
    update_led_strip();
}

void set_led(int led_num, Color color){
    uint32_t col = (color.green << 16) | (color.red << 8) | color.blue;
    colors[led_num] = col;
}

void set_key(int key_num, int stat, Color color){
    Key current_key = key_array[key_num];
    if(stat == 0){
        color = (Color) {.red = 0, .green = 0, .blue = 0};
    }
    for(int i = current_key.starting_led; i < current_key.starting_led + current_key.num_led; i++){
        set_led(i, color);
    }
    update_led_strip();
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
  update_led_strip();
}

void initialize_led_strip(int num, int pin){
    int num_keys = 88; // Todo take this in as an argument
    num_leds = num;
    led_pin = pin;
    update_finished = 1;
    current_led = num_leds + 40;
    bufferA = malloc(sizeof(uint16_t) * 24);
    bufferB = malloc(sizeof(uint16_t) * 24);
    colors = malloc(sizeof(*colors) * num_leds);
    key_array = malloc(sizeof(*key_array) * num_keys);
    setup_key_array(num_keys);
    setup_led_pwm_dma();
    NRF_PWM0 -> TASKS_SEQSTART[0] = 1;
    fill_color((Color) {.red=0, .green=0, .blue=0});
    //fill_test();
    //setup_led_refresh(2);
}