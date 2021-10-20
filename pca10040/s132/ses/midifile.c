#include <stdint.h>
#include "midifile.h"

uint8_t get_next_byte(){
    return 0;
}

MidiEvent get_next_midi_event(){
    uint8_t newid = get_next_byte();
    uint8_t newnote = get_next_byte();
    uint8_t vel = get_next_byte();
    return (MidiEvent) {.ID = newid, .note=newnote, .velocity=vel};
}

unsigned long get_next_midi_delay(){
    unsigned long duration = 0;
    
    do {
        duration <<= 7;
        duration |= get_next_byte()
    } while(duration & 0x80);

    return duration;
}

void read_next_midi_data(){
    MidiEvent updates[MIDI_EVENT_LIMIT] = {0};
    int events_read = 0;
    uint16_t delay = 0

    while(!delay && events_read < MIDI_EVENT_LIMIT) {
        MidiEvent evt = get_next_midi_event();
        delay = get_next_midi_delay();
        updates[events_read] = evt;
        events_read++;
    }
    // Update leds based on updates
    for(int i = 0; i < MIDI_EVENT_LIMIT; i++){
        MidiEvent curr = updates[i];
        if(curr == 0){
            break;
        }
        int stat = curr.ID == 0x90 ? 1 : 0;
        set_key_velocity(curr.note, stat, curr.velocity);
    }
    update_led_strip()
    // TODO: wait delay, then schedule read_next_midi_data
}