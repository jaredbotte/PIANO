#pragma once
#include "ff.h"
#include "leds.h"

#define MIDI_EVENT_LIMIT 10

typedef struct {
    uint8_t ID;
    uint8_t note;
    uint8_t velocity;
} MidiEvent;

typedef struct {
    FIL* ptr;
    uint16_t format;
    uint16_t numTracks;
    uint16_t division;
    int useconds_per_tick;
    uint32_t tempo; // uS/qn
} MidiFile;

static MidiFile midi_file;


void read_next_midi_data();
MidiEvent get_midi_event(uint8_t newid);
MetaEvent get_meta_event();
unsigned long get_variable_data();
uint8_t get_next_byte();
void init_midi_file(filename);
