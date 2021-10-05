#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "leds.h"
#include "nrf.h"
#include "nrf_delay.h"

int main(void) {   
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

