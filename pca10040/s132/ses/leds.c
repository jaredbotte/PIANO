#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "leds.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"


//Globals
int num_leds, led_pin, num_keys;
uint16_t* buffer;
Key* key_array;


//--- Interrupt Handlers ---//

//PWM handler when new data is recieved via DMA
void PWM0_IRQHandler(){
   NRF_PWM0 -> EVENTS_SEQEND[0] = 0;
   while(NRF_PWM0 -> EVENTS_SEQEND[0]);
}


//BLE Advertising blink handler
void TIMER4_IRQHandler(void)
{
  NRF_TIMER4->EVENTS_COMPARE[0] = 0;	
  nrf_gpio_pin_toggle(LED_BLUE);
}


//--- Setup Functions ---//

//Initializes DMA for PWM
void setup_led_pwm_dma() {
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


//Init timers for updaing LED strip and BLE blinking
void start_timer() {		
  NRF_TIMER3->MODE = TIMER_MODE_MODE_Timer;
  NRF_TIMER3->TASKS_CLEAR = 1;
  NRF_TIMER3->PRESCALER = 3; //cannot display 1/128 notes proerly
  NRF_TIMER3->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
  NRF_TIMER3->CC[0] = 10000;
  NRF_TIMER3->INTENSET = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
  NVIC_EnableIRQ(TIMER3_IRQn);

  NRF_TIMER4->MODE = TIMER_MODE_MODE_Timer;
  NRF_TIMER4->TASKS_CLEAR = 1;
  NRF_TIMER4->PRESCALER = 7;
  NRF_TIMER4->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
  NRF_TIMER4->CC[0] = 10000;
  NRF_TIMER4->INTENSET = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
  NVIC_EnableIRQ(TIMER4_IRQn);

  NRF_TIMER3->TASKS_START = 1;
  NRF_TIMER4->TASKS_START = 1;
}


//Inits on-board LED's
void initIndication() {
  nrf_gpio_cfg_output(LED_BLUE);
  nrf_gpio_cfg_output(LED_GREEN);
  ledIndicate(-1);
  ledIndicate(LED_POWER);
}


//Initializes the key_array
void setup_key_array(int num_keys){
    int key_led = 0;
    int width = 2; // For now we'll make each key two LEDs wide. Actually pretty darn close!
    for(int k = 0; k < num_keys; k++){
        if(k == 88 - 16){
            key_led--;
        }
        key_array[k] = (Key) {.starting_led = key_led, .num_led = width, .systemLit = false .userLit = false};
        key_led += width;
    }
}

//Initializes LED strip for operation
void initialize_led_strip(int ledNum, int pinNum, int keyNum){
    num_keys = keyNum;
    num_leds = ledNum;
    led_pin = pinNum;

    //Critical Memory Allocation
    buffer = malloc(sizeof(*buffer) * num_leds * 24 + sizeof(*buffer) * 40);
    if (buffer == NULL) {
        printf("Memory Limit Reached, LED Buffer allocation failed :(\r\n");
        return;
    }
    key_array = malloc(sizeof(*key_array) * num_keys);
    if (key_array == NULL) {
        printf("Memory Limit Reached, Key Array allocation failed :(\r\n");
        return;
    }
    //End Critical Memory Allocation

    setup_key_array(num_keys);
    for(int i = num_leds * 24; i < num_leds * 24 + 40; i++){
        buffer[i] = 0x8000;
    }
    setup_led_pwm_dma();
    fill_color(OFF);
    start_timer();

    //Test Functions
    //fill_test();
}



void update_led_strip(){
    NRF_PWM0 -> TASKS_SEQSTART[0] = 1;
}


/*
//Polls the key_array for led changes and then pushes an update (quite slow)
//NOTE NOT IN USE
void updateKeys() {
    bool keyChanged = false //this should be a global, but put here since not in use
    if(keyChanged) {
        for(int i = 0; i < num_keys; i++) {
            Key current_key = key_array[i];
            for(int j = current_key.starting_led; j < current_key.starting_led + current_key.num_led; j++){
                set_led(j, current_key.color);
            }
        }
        keyChanged = false;
    }
    update_led_strip();
}
*/


//Controls on-board indication LED's
void ledIndicate(int action) {
  NRF_TIMER4->TASKS_STOP = 1;
  switch(action) {
    case LED_POWER:
      nrf_gpio_pin_set(LED_GREEN);
      break;
    case LED_IDLE:
      nrf_gpio_pin_clear(LED_BLUE);
      break;
    case LED_ADVERTISING:
      NRF_TIMER4->TASKS_START = 1;
      break;
    case LED_CONNECTED:
      nrf_gpio_pin_set(LED_BLUE);
      break;
    default:
      nrf_gpio_pin_clear(LED_GREEN);
      nrf_gpio_pin_clear(LED_BLUE);
  }
}


//Sets the color of a single LED
void set_led(int led_num, Color color){
    uint32_t col = (color.green << 16) | (color.red << 8) | color.blue;
    int led_ind = led_num * 24;
    for(int i = 0; i < 24; i++) {
        buffer[led_ind + i] = (((col & (1 << (23 - i))) >> (23 - i)) * 6 + 6) | 0x8000;
    }
}


//Checks if 2 colors are the same
bool areSameColor(Color a, Color b){
    return (a.red == b.red && a.green == b.green && a.blue == b.blue);
}  


//Returns an LED's color
Color get_led_color(int led_num){
    uint32_t col = 0;
    int led_ind = led_num *24;
    for(int i = 0; i < 24; i++){
        uint16_t val = buffer[led_ind + i];
        if(val == 0x8006){
            val = 0;
        } else {
            val = 1;
        }
        col <<= 1;
        col |= val;
    }
    return (Color) {.green = (col >> 16) & 0xff, .red = (col >> 8) & 0Xff, .blue = col & 0xff};
}


//Fills the LED strip with 1 color
void fill_color(Color color){
    for(int n = 0; n < num_leds; n++){
        set_led(n, color);
    }
}


//Fills LED strip wil alternating red and blue
void fill_test(){
  for(int n = 0; n < num_leds; n++){
     if (n % 2 == 0) {
      set_led(n, RED);
    } else {
      set_led(n, BLUE);
    }
  }
}


//--- High Level Key Functions ---//

//Sets a key's color and the origin of the color (user or system)
void set_key(uint8_t key_num, bool keyOn, bool isSystemLit, int velocity, Color color){
    if(key_num >= 88){
        return;
    }
    Key* current_key = &key_array[key_num];
    if(!keyOn || velocity == 0){
        color = OFF;
        if(isSystemLit) {
            current_key->systemLit = false;
        }
        else {
            current_key->userLit = false;
        }
    }
    else {
        if(isSystemLit) {
            current_key->systemLit = true;
        }
        else {
            current_key->userLit = true;
        }
    }

    for(int j = current_key->starting_led; j < current_key->starting_led + current_key->num_led; j++){
      set_led(j, color);
    }
}


//Sets a key's color based on velocity 
//NOTE: This functuon should only be used in PA/VIS mode due to isSystemLit being set to true in set_key
void set_key_velocity(uint8_t key_num, bool keyOn, int velocity){
    Color color = (Color) {.red = 0, .green = 63, .blue = 0};
    if(!keyOn) {
        color = OFF;
    } 
    else if (velocity > 127.0 * 0.6) {
        color = RED;
    } 
    else if (velocity > 127.0 * 0.3) {
        color = ORANGE;
    } 
    else {
        color = GREEN;
    }
    set_key(key_num, keyOn, true, velocity, color);
}


//Does nothing right now
void set_key_play(uint8_t key_num, bool keyOn, int velocity){ //TODO 
    // NOTE: We can make this do something if we want.
    // This function is called in PA Mode when a key is pressed.
    ;
}



//Sets key lights in LTP mode
void set_key_learn(uint8_t key_num, bool keyOn, int velocity){
    Key* current_key = &key_array[key_num];
    if(keyOn) {
        if(current_key->systemLit) {
            set_key(key_num, true, false, velocity, CORRECT_COLOR);
        }
        else {
            set_key(key_num, true, false, velocity, INCORRECT_COLOR);
        }
    } 
    else {
        if(current_key->systemLit) {
            current_key->userLit = false;
            set_key(key_num, true, true, 1, LEARN_COLOR); //letting go of the key softly should never trigger an off event
        }
        else {
            set_key(key_num, false, false, velocity, OFF);
        }
    }
}


//Returns a key's color
Color get_key_color(uint8_t key_num){
    return get_led_color(key_array[key_num].starting_led);
}


//Returns if the current LTP note set is complete
bool isLearnSetFinished(){ //TODO Add feedback (like blink the leds) instead of just setting them
    bool finished = true;
    for(int i = 0; i < num_keys; i++) {
        Key current_key = key_array[i];
        if((current_key.systemLit && !current_key.userLit)) { //User hasn't pressed a key
            finished = false;
            set_key(i, true, true, 1, LEARN_COLOR);
        }
        if((!current_key.systemLit && current_key.userLit)) { //User is pressing an incorrect key
            finished = false;
            set_key(i, true, false, 1, INCORRECT_COLOR);
        }
    }
    return finished;
}

void resetKeys() {
    for(int i = 0; i < num_keys; i++) {
        key_array[i].systemLit = false;
        key_array[i].userLit = false;        
    }
}


//Plays an animation on the LED strip upon BLE connection
void led_connect_animation(){ //TODO
    // Will need to figure out how to delay something so we can make an animation. ~ just put the system into PA and play a file hidden from the user
    //fill_color(RED);
}