#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "leds.h"
#include "nrf.h"

int num_leds;
int led_pin;
int update_finished;
int current_led;
uint32_t* colors;
uint16_t* bufferA;
uint16_t* bufferB;

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

void initialize_led_strip(int num, int pin){
    num_leds = num;
    led_pin = pin;
    update_finished = 1;
    current_led = num_leds + 40;
    bufferA = malloc(sizeof(uint16_t) * 24);
    bufferB = malloc(sizeof(uint16_t) * 24);
    colors = malloc(sizeof(*colors) * num_leds);
    setup_led_pwm_dma();
    NRF_PWM0 -> TASKS_SEQSTART[0] = 1;
    fill_color((Color) {.red=0, .green=0, .blue=0});
}