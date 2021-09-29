#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "leds.h"
#include "nrf.h"

int num_leds;
int led_pin;
int update_finished;
uint16_t* buffer;

void PWM0_IRQHandler(){
    // This handles the DMA transfer complete interrupts.
    NRF_PWM0 -> EVENTS_SEQEND[0] = 0; // Reset the interrupt
    while(NRF_PWM0 -> EVENTS_SEQEND[0]); // Wait for the interrupt to be cleared
    update_finished = 1; // Update has completed
}

static void setup_led_pwm_dma(){
    // GPIO setup
    NRF_GPIO -> DIRSET = (1 << led_pin);  // set pin to output
    NRF_GPIO -> OUTCLR = (1 << led_pin);  // set pin to pull low when not in use

    // PWM setup
    NRF_PWM0 -> PSEL.OUT[0] = led_pin;  // Set PWM0 Channel 0 to output on led_pin
    NRF_PWM0 -> ENABLE = PWM_ENABLE_ENABLE_Enabled << PWM_ENABLE_ENABLE_Pos; // Enable PWM0
    NRF_PWM0 -> MODE = PWM_MODE_UPDOWN_Up << PWM_MODE_UPDOWN_Pos; // Set PWM mode to upcounter
    NRF_PWM0 -> PRESCALER = 0;  // Use the full 16MHz clock
    NRF_PWM0 -> COUNTERTOP = 20;  // 1.25 us period
    NRF_PWM0 -> LOOP = 0;  // Don't loop
    NRF_PWM0 -> DECODER = PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos | PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos;
    
    // SEQ[0] Will be our main buffer. Last 40 elements are the latch
    NRF_PWM0 -> SEQ[0].PTR = (uint32_t) buffer;  // The pointer to the buffer
    NRF_PWM0 -> SEQ[0].CNT = (24 * num_leds) + 40;  // Number of elements in the buffer
    NRF_PWM0 -> SEQ[0].REFRESH = 0;  // Send each element only once
    NRF_PWM0 -> SEQ[0].ENDDELAY = 0;  // Don't delay after finishing

    // Interrupt setup
    NVIC -> ISER[0] = 1 << PWM0_IRQn;
    NRF_PWM0 -> INTENSET = PWM_INTENSET_SEQEND0_Enabled << PWM_INTENSET_SEQEND0_Pos;
}

void update_led_strip(){
    while(update_finished == 0){
        __asm__("nop");
    }
    update_finished = 0;
    NRF_PWM0 -> TASKS_SEQSTART[0] = 1;
}

void fill_color(Color color){
    for(int n = 0; n < num_leds; n++){
        set_led(n, color);
    }
    update_led_strip();
}

void set_led(int led_num, Color color){
    uint32_t col = (color.green << 16) | (color.red << 8) | color.blue;
    for(int i = 0; i < 24; i++){
      int led_ind = led_num * 24;
        buffer[led_ind + i] = (((col & (1 << (23 - i))) >> (23 - i)) * 6 + 6) | 0x8000;
    }
}

void initialize_led_strip(int num, int pin){
    num_leds = num;
    led_pin = pin;
    update_finished = 1;

    buffer = malloc(sizeof(*buffer) * num_leds * 24 + sizeof(*buffer) * 40);
    for(int i = num_leds * 24; i < num_leds * 24 + 40; i++){
        buffer[i] = 0x8000;
    }
    setup_led_pwm_dma();
    fill_color((Color) {.red=0, .green=0, .blue=0});
}