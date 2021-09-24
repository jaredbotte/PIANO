#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "leds.h"
#include "nrf.h"
#include "nrf_delay.h"

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
    //blink_led(10);
    initialize_led_strip(144, 17);

    while (true){
        //main loop
        Color red = {.red = 63, .green = 0, .blue = 0};
        Color yellow = {.red = 63, .green = 63, .blue = 0};
        Color green = {.red = 0, .green = 63, .blue = 0};
        Color light_blue = {.red = 0, .green = 63, .blue = 63};
        Color blue = {.red = 0, .green = 0, .blue = 63};
        Color purple = {.red = 63, .green = 0, .blue = 63};
        /*nrf_delay_ms(1000);
        fill_color(red);
        nrf_delay_ms(1000);
        fill_color(yellow);
        nrf_delay_ms(1000);
        fill_color(green);
        nrf_delay_ms(1000);
        fill_color(light_blue);
        nrf_delay_ms(1000);
        fill_color(blue);
        nrf_delay_ms(1000);
        fill_color(purple);*/
        set_led(2, red);
        set_led(4, green);
        set_led(6, blue);
    }
}

