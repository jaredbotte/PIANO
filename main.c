#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "leds.h"
#include "nrf.h"

void blink_led(int rate_hz){
  // This function will blink one of the LEDs at the rate specified by rate_hz
  // Maximum time of 2^32 / f_timer = 33.55 seconds

  NRF_GPIO -> DIRSET = GPIO_DIRSET_PIN20_Output << GPIO_DIRSET_PIN20_Pos;

  NRF_TIMER3 -> TASKS_STOP = 1;
  NRF_TIMER3 -> MODE = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
  NRF_TIMER3 -> BITMODE = TIMER_BITMODE_BITMODE_24Bit << TIMER_BITMODE_BITMODE_Pos;
  NRF_TIMER3 -> PRESCALER = 5; //f_timer = 16,000,000 / 2 ^ 5 = 500,000
  NRF_TIMER3 -> TASKS_CLEAR = 1;
  NRF_TIMER3 -> CC[0] = 500000 / (rate_hz * 2);

  NRF_TIMER3 -> INTENSET = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;
  NRF_TIMER3 -> SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;
  NVIC -> ISER[0] = 1 << TIMER3_IRQn;
  NRF_TIMER3 -> TASKS_START = 1;
}

void TIMER3_IRQHandler(){
  // Check the status of the pin
  NRF_TIMER3 -> EVENTS_COMPARE[0] = 0;
  if(NRF_GPIO -> OUT & GPIO_OUT_PIN20_Msk){
    NRF_GPIO -> OUTCLR = 1 << GPIO_OUTCLR_PIN20_Pos;
  } else {
    NRF_GPIO -> OUTSET = 1 << GPIO_OUTSET_PIN20_Pos;
  }
  while(NRF_TIMER3 -> EVENTS_COMPARE[0]);
}

int main(void) {   
    blink_led(2);
    initialize_led_strip(1, 17);
    fill_color((Color) {.red=255, .green=0, .blue=0});

    while (true){
        //main loop
        __asm__("wfi");
    }
}

