#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "leds.h"
#include "nrf.h"
#include "nrf_delay.h" // leds, uart

int num_leds;
int led_pin;
uint16_t* buffer;
Key* key_array;

void PWM0_IRQHandler(){
    // This handles the DMA transfer complete interrupts.
   NRF_PWM0 -> EVENTS_SEQEND[0] = 0; // Reset the interrupt
   while(NRF_PWM0 -> EVENTS_SEQEND[0]); // Wait for the interrupt to be cleared
}


void start_timer(void)
{		
  NRF_TIMER3->MODE = TIMER_MODE_MODE_Timer;  // Set the timer in Counter Mode
  NRF_TIMER3->TASKS_CLEAR = 1;               // clear the task first to be usable for later
  NRF_TIMER3->PRESCALER = 3;                             //Set prescaler. Higher number gives slower timer. Prescaler = 0 gives 16MHz timer
  NRF_TIMER3->BITMODE = TIMER_BITMODE_BITMODE_16Bit;		 //Set counter to 16 bit resolution
  NRF_TIMER3->CC[0] = 10000;                             //Set value for TIMER2 compare register 0????????? DOES THIS WORK????
  NRF_TIMER3->INTENSET = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
  NVIC_EnableIRQ(TIMER3_IRQn);
		
  NRF_TIMER3->TASKS_START = 1;               // Start TIMER2
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
        key_array[k] = (Key) {.starting_led = key_led, .num_led = width};
        key_led += width;
    }
}

void update_led_strip(){
    NRF_PWM0 -> TASKS_SEQSTART[0] = 1;
}

void fill_color(Color color){
    for(int n = 0; n < num_leds; n++){
        set_led(n, color);
    }
}

void set_led(int led_num, Color color){
    uint32_t col = (color.green << 16) | (color.red << 8) | color.blue;
    for(int i = 0; i < 24; i++){
      int led_ind = led_num * 24;
        buffer[led_ind + i] = (((col & (1 << (23 - i))) >> (23 - i)) * 6 + 6) | 0x8000;
    }
}

void set_key(int key_num, int stat, Color color){
    Key current_key = key_array[key_num];
    if(stat == 0){
        color = (Color) {.red = 0, .green = 0, .blue = 0};
    }
    for(int i = current_key.starting_led; i < current_key.starting_led + current_key.num_led; i++){
        set_led(i, color);
    }
}

void set_key_velocity(int key_num, int stat, int velocity){
    Key current_key = key_array[key_num];
    Color color = (Color) {.red = 0, .green = 63, .blue = 0};
    if(stat == 0) {
        color = OFF;
    } else if (velocity > 127.0 * 0.6) {
        color = RED;
    } else if (velocity > 127.0 * 0.3) {
        color = ORANGE;
    } else {
        color = GREEN;
    }

    for(int i = current_key.starting_led; i < current_key.starting_led + current_key.num_led; i++){
        set_led(i, color);
    }
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

void initialize_led_strip(int num, int pin){
    int num_keys = 88; // Todo take this in as an argument
    num_leds = num;
    led_pin = pin;
    buffer = malloc(sizeof(*buffer) * num_leds * 24 + sizeof(*buffer) * 40);
    key_array = malloc(sizeof(*key_array) * num_keys);
    setup_key_array(num_keys);
    for(int i = num_leds * 24; i < num_leds * 24 + 40; i++){
        buffer[i] = 0x8000;
    }
    setup_led_pwm_dma();
    fill_color((Color) {.red=0, .green=0, .blue=0});
    start_timer();
    //fill_test();
}