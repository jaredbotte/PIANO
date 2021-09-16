#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "leds.h"
#include "nrf.h"

int main(void) {
   initialize_led_strip(1, 17);
   //fill_color((Color) {.red=255, .green=0, .blue=0});
   //update_led_strip();

    /*uint16_t* buffer = malloc(sizeof(*buffer) * 24);
    for(int i = 0; i < 8; i++){
        buffer[i] = 12 | 0x8000;
    }
    for(int i = 8; i < 24; i++){
        buffer[i] = 6 | 0x8000;
    }*/

    while (true){
        //main loop
        __asm__("wfi");
    }
}

