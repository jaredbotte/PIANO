#include <stdint.h>
#include "midifile.h"

uint8_t get_next_byte(){
    uint8_t byte = 0;
    uint8_t bytes_read = 0;
    FRESULT res = f_read(midi_file.ptr, &byte, 1, &br);
    return byte;
}

MidiEvent get_midi_event(uint8_t newid){
    uint8_t note_info[2] = {0};
    uint8_8 br = 0;
    FRESULT res = f_read(midi_file.ptr, note_info, 2, &br);
    uint8_t newnote = note_info[0];
    uint8_t vel = note_info[1];
    return (MidiEvent) {.ID = newid, .note=newnote, .velocity=vel};
}

void get_meta_event(){
    uint8_t meta_type = get_next_byte();
    unsigned long length = get_variable_data();
    if(meta_type == 0x2F){ // End of track
        // TODO: Change state of system!
    } else if (meta_type == 0x51){ // Tempo setting
        // Length should be 3!
        uint8_t tempodat[3] = {0};
        uint8_t br;
        f_read(midi_file.ptr, tempodat, length, &br);
        // TODO: set tempo!
    } else if (meta_type == 0x54) { // SMPTE offset

    } else if (meta_type == 0x58) { // Time offset

    } else {
        f_lseek(midi_file.ptr, length);
    }
}

unsigned long get_variable_data(){
    unsigned long v_data = 0;
    
    do {
        v_data <<= 7;
        next_byte = get_next_byte()
        uint8_t more_bytes = next_byte & 0x80;
        v_data |= next_byte & 0x7f;
    } while(more_bytes);

    return v_data;
}

uint8_t read_next_track_event(){
    uint8_t evt = get_next_byte();
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
    // NOTE: Do we want to update the LED strip here or keep our constant refresh rate?
    // TODO: wait delay, then schedule read_next_midi_data
}

void init_midi_file(filename){
    FRESULT res = f_open(midi_file.ptr, filename, FA_READ);
    if(res != FR_OK){
        return; // We've encountered an error
    }
    uint8_t header[6] = {0};
    uint8_t br = 0;
    res = f_read(midi_file.ptr, header, 6, &br);
    midi_file.format = header[0] << 8 | header[1];
    midi_file.numTracks = header[2] << 8 | header[3];
    midi_file.division = header[4] << 8 | header[5];
    // NEED seconds/tick!
    // NOTE: Remove the first delay.
}