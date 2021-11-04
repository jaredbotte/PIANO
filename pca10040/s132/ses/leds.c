#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "leds.h"
#include "nrf.h"
#include "nrf_delay.h" // leds, uart

#define PIANO_LEDS_START 6

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

void set_key_learn(int key_num, int stat){
    /*if (correctKey && keyPressed){
        light key green;
    } else if (correctKey & !keyPressed){
        light key blue;
    } else if (!correctKey & keyPressed) {
        light key red;
    } else if (!correctKey & !keyPressed) {
        light key off;
    }*/

}

void set_key_play(int key_num, int stat){

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

    //Init onboard LEDs
    NRF_GPIO -> DIR |= (3 << PIANO_LEDS_START);
    NRF_GPIO -> OUTSET |= (1 << PIANO_LEDS_START + 1);
}

/*
//BLE Indication functions
static bool m_leds_clear = false;
uint32_t ble_indication_set(bsp_indication_t indicate)
{
    uint32_t err_code = NRF_SUCCESS;
    err_code = bsp_led_indication(indicate);
    return err_code;
}


static uint32_t ble_led_indication(bsp_indication_t indicate)
{
    uint32_t err_code   = NRF_SUCCESS;
    uint32_t next_delay = 0;

    if (m_leds_clear)
    {
        m_leds_clear = false;
        NRF_GPIO -> OUTCLR |= 1 << PIANO_LEDS_START;
    }

    switch (indicate)
    {
        case BSP_INDICATE_IDLE:
            NRF_GPIO -> OUTCLR |= 1 << PIANO_LEDS_START;;
            err_code       = app_timer_stop(m_bsp_leds_tmr);
            m_stable_state = indicate;
            break;

        case BSP_INDICATE_SCANNING:
        case BSP_INDICATE_ADVERTISING:
            // in advertising blink LED_0
            if (bsp_board_led_state_get(BSP_LED_INDICATE_INDICATE_ADVERTISING))
            {
                bsp_board_led_off(BSP_LED_INDICATE_INDICATE_ADVERTISING);
                next_delay = indicate ==
                             BSP_INDICATE_ADVERTISING ? ADVERTISING_LED_OFF_INTERVAL :
                             ADVERTISING_SLOW_LED_OFF_INTERVAL;
            }
            else
            {
                bsp_board_led_on(BSP_LED_INDICATE_INDICATE_ADVERTISING);
                next_delay = indicate ==
                             BSP_INDICATE_ADVERTISING ? ADVERTISING_LED_ON_INTERVAL :
                             ADVERTISING_SLOW_LED_ON_INTERVAL;
            }

            m_stable_state = indicate;
            err_code       = app_timer_start(m_bsp_leds_tmr, APP_TIMER_TICKS(next_delay), NULL);
            break;

        case BSP_INDICATE_ADVERTISING_WHITELIST:
            // in advertising quickly blink LED_0
            if (bsp_board_led_state_get(BSP_LED_INDICATE_ADVERTISING_WHITELIST))
            {
                bsp_board_led_off(BSP_LED_INDICATE_ADVERTISING_WHITELIST);
                next_delay = indicate ==
                             BSP_INDICATE_ADVERTISING_WHITELIST ?
                             ADVERTISING_WHITELIST_LED_OFF_INTERVAL :
                             ADVERTISING_SLOW_LED_OFF_INTERVAL;
            }
            else
            {
                bsp_board_led_on(BSP_LED_INDICATE_ADVERTISING_WHITELIST);
                next_delay = indicate ==
                             BSP_INDICATE_ADVERTISING_WHITELIST ?
                             ADVERTISING_WHITELIST_LED_ON_INTERVAL :
                             ADVERTISING_SLOW_LED_ON_INTERVAL;
            }
            m_stable_state = indicate;
            err_code       = app_timer_start(m_bsp_leds_tmr, APP_TIMER_TICKS(next_delay), NULL);
            break;

        case BSP_INDICATE_ADVERTISING_SLOW:
            // in advertising slowly blink LED_0
            if (bsp_board_led_state_get(BSP_LED_INDICATE_ADVERTISING_SLOW))
            {
                bsp_board_led_off(BSP_LED_INDICATE_ADVERTISING_SLOW);
                next_delay = indicate ==
                             BSP_INDICATE_ADVERTISING_SLOW ? ADVERTISING_SLOW_LED_OFF_INTERVAL :
                             ADVERTISING_SLOW_LED_OFF_INTERVAL;
            }
            else
            {
                bsp_board_led_on(BSP_LED_INDICATE_ADVERTISING_SLOW);
                next_delay = indicate ==
                             BSP_INDICATE_ADVERTISING_SLOW ? ADVERTISING_SLOW_LED_ON_INTERVAL :
                             ADVERTISING_SLOW_LED_ON_INTERVAL;
            }
            m_stable_state = indicate;
            err_code       = app_timer_start(m_bsp_leds_tmr, APP_TIMER_TICKS(next_delay), NULL);
            break;

        case BSP_INDICATE_ADVERTISING_DIRECTED:
            // in advertising very quickly blink LED_0
            if (bsp_board_led_state_get(BSP_LED_INDICATE_ADVERTISING_DIRECTED))
            {
                bsp_board_led_off(BSP_LED_INDICATE_ADVERTISING_DIRECTED);
                next_delay = indicate ==
                             BSP_INDICATE_ADVERTISING_DIRECTED ?
                             ADVERTISING_DIRECTED_LED_OFF_INTERVAL :
                             ADVERTISING_SLOW_LED_OFF_INTERVAL;
            }
            else
            {
                bsp_board_led_on(BSP_LED_INDICATE_ADVERTISING_DIRECTED);
                next_delay = indicate ==
                             BSP_INDICATE_ADVERTISING_DIRECTED ?
                             ADVERTISING_DIRECTED_LED_ON_INTERVAL :
                             ADVERTISING_SLOW_LED_ON_INTERVAL;
            }
            m_stable_state = indicate;
            err_code       = app_timer_start(m_bsp_leds_tmr, APP_TIMER_TICKS(next_delay), NULL);
            break;

        case BSP_INDICATE_BONDING:
            // in bonding fast blink LED_0
            bsp_board_led_invert(BSP_LED_INDICATE_BONDING);

            m_stable_state = indicate;
            err_code       =
                app_timer_start(m_bsp_leds_tmr, APP_TIMER_TICKS(BONDING_INTERVAL), NULL);
            break;

        case BSP_INDICATE_CONNECTED:
            bsp_board_led_on(BSP_LED_INDICATE_CONNECTED);
            m_stable_state = indicate;
            break;

        case BSP_INDICATE_SENT_OK:
            // when sending shortly invert LED_1
            m_leds_clear = true;
            bsp_board_led_invert(BSP_LED_INDICATE_SENT_OK);
            err_code = app_timer_start(m_bsp_leds_tmr, APP_TIMER_TICKS(SENT_OK_INTERVAL), NULL);
            break;

        case BSP_INDICATE_SEND_ERROR:
            // on receving error invert LED_1 for long time
            m_leds_clear = true;
            bsp_board_led_invert(BSP_LED_INDICATE_SEND_ERROR);
            err_code = app_timer_start(m_bsp_leds_tmr, APP_TIMER_TICKS(SEND_ERROR_INTERVAL), NULL);
            break;

        case BSP_INDICATE_RCV_OK:
            // when receving shortly invert LED_1
            m_leds_clear = true;
            bsp_board_led_invert(BSP_LED_INDICATE_RCV_OK);
            err_code = app_timer_start(m_bsp_leds_tmr, APP_TIMER_TICKS(RCV_OK_INTERVAL), NULL);
            break;

        case BSP_INDICATE_RCV_ERROR:
            // on receving error invert LED_1 for long time
            m_leds_clear = true;
            bsp_board_led_invert(BSP_LED_INDICATE_RCV_ERROR);
            err_code = app_timer_start(m_bsp_leds_tmr, APP_TIMER_TICKS(RCV_ERROR_INTERVAL), NULL);
            break;

        case BSP_INDICATE_FATAL_ERROR:
            // on fatal error turn on all leds
            bsp_board_leds_on();
            m_stable_state = indicate;
            break;

        case BSP_INDICATE_ALERT_0:
        case BSP_INDICATE_ALERT_1:
        case BSP_INDICATE_ALERT_2:
        case BSP_INDICATE_ALERT_3:
        case BSP_INDICATE_ALERT_OFF:
            err_code   = app_timer_stop(m_bsp_alert_tmr);
            next_delay = (uint32_t)BSP_INDICATE_ALERT_OFF - (uint32_t)indicate;

            // a little trick to find out that if it did not fall through ALERT_OFF
            if (next_delay && (err_code == NRF_SUCCESS))
            {
                if (next_delay > 1)
                {
                    err_code = app_timer_start(m_bsp_alert_tmr,
                                               APP_TIMER_TICKS(((uint16_t)next_delay * ALERT_INTERVAL)),
                                               NULL);
                }
                bsp_board_led_on(BSP_LED_ALERT);
                m_alert_on = true;
            }
            else
            {
                bsp_board_led_off(BSP_LED_ALERT);
                m_alert_on = false;

            }
            break;

        case BSP_INDICATE_USER_STATE_OFF:
            leds_off();
            m_stable_state = indicate;
            break;

        case BSP_INDICATE_USER_STATE_0:
            leds_off();
            bsp_board_led_on(BSP_LED_INDICATE_USER_LED1);
            m_stable_state = indicate;
            break;

        case BSP_INDICATE_USER_STATE_1:
            leds_off();
            bsp_board_led_on(BSP_LED_INDICATE_USER_LED2);
            m_stable_state = indicate;
            break;

        case BSP_INDICATE_USER_STATE_2:
            leds_off();
            bsp_board_led_on(BSP_LED_INDICATE_USER_LED1);
            bsp_board_led_on(BSP_LED_INDICATE_USER_LED2);
            m_stable_state = indicate;
            break;

        case BSP_INDICATE_USER_STATE_3:

        case BSP_INDICATE_USER_STATE_ON:
            bsp_board_leds_on();
            m_stable_state = indicate;
            break;

        default:
            break;
    }

    return err_code;
}*/