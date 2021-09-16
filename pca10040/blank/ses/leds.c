#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "leds.h"
#include "nrf.h"

int num_leds;
int led_pin;
int update_finished;
uint16_t latch;
uint16_t* buffer;

void PWM0_IRQHandler(){
    // This handles the DMA transfer complete interrupts.
    if(NRF_PWM0 -> EVENTS_SEQEND[0]){
        NRF_PWM0 -> EVENTS_SEQEND[0] = 0; // Reset the interrupt
        //NRF_PWM0 -> TASKS_STOP = 1; // Stop PWM, will force a LOW state.
        NRF_PWM0 -> TASKS_SEQSTART[1] = 1; // Start the latch
        while(NRF_PWM0 -> EVENTS_SEQEND[0]); // Wait for the interrupt to be cleared
    } else {
        NRF_PWM0 -> EVENTS_SEQEND[1] = 0;  // Reset the interrupt
        NRF_PWM0 -> TASKS_STOP = 1;  // Stop PWM, will force a LOW state.
        while(NRF_PWM0 -> EVENTS_SEQEND[1]);  // Wait for interrupt to clear
        update_finished = 1; // Update has completed
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
    NRF_PWM0 -> PRESCALER = 0;  // Use the full 16MHz clock
    NRF_PWM0 -> COUNTERTOP = 20;  // 1.25 us period
    NRF_PWM0 -> LOOP = 0;  // Don't loop
    NRF_PWM0 -> DECODER = PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos | PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos;
    
    // SEQ[0] Will be our main buffer
    NRF_PWM0 -> SEQ[0].PTR = (uint32_t) buffer;  // The pointer to the buffer
    NRF_PWM0 -> SEQ[0].CNT = 24 * num_leds;  // Number of elements in the buffer
    NRF_PWM0 -> SEQ[0].REFRESH = 0;  // Send each element only once
    NRF_PWM0 -> SEQ[0].ENDDELAY = 0;  // Don't delay after finishing

    // SEQ[1] Will be our latch
    latch = 0;
    NRF_PWM0 -> SEQ[1].PTR = (uint32_t) &latch; // Pointer to latch
    NRF_PWM0 -> SEQ[1].CNT = 1;  // One element
    NRF_PWM0 -> SEQ[1].REFRESH = 40;  // Send element 40 times
    NRF_PWM0 -> SEQ[1].ENDDELAY = 0;  // Don't repeat

    // Interrupt setup
    NVIC -> ISER[0] = 1 << PWM0_IRQn;
    NRF_PWM0 -> INTENSET = PWM_INTENSET_SEQEND0_Enabled << PWM_INTENSET_SEQEND0_Pos;
    NRF_PWM0 -> INTENSET = PWM_INTENSET_SEQEND1_Enabled << PWM_INTENSET_SEQEND1_Pos;
}

void update_led_strip(){
    while(!update_finished);
    update_finished = 0;
    NRF_PWM0 -> TASKS_SEQSTART[0] = 1;
}

void fill_color(Color color){
    for(int n = 0; n < num_leds; n++){
        set_led(n, color);
    }
    update_led_strip()
}

void set_led(int led_num, Color color){
    uint32_t col = (color.green << 16) | (color.red << 8) | color.blue;
    for(int i = 0; i < 24; i++){
        buffer[(led_num * 24) + i] = ((col & (1 << (23 - i))) >> (23 - i)) * 19 + 19;
    }
}

void initialize_led_strip(int num, int pin){
    num_leds = num;
    led_pin = pin;
    update_finished = 1;

    buffer = malloc(sizeof(*buffer) * num_leds * 24);

    setup_led_pwm_dma();
    fill_color((Color) {.red=0, .green=0, .blue=0});
}