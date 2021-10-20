#pragma once
#include "ff.h"
#include "leds.h"

#define MIDI_EVENT_LIMIT 10

typedef struct {
  uint8_t ID;
  uint8_t note;
  uint8_t velocity;
} MidiEvent;

void read_next_midi_data();
MidiEvent get_next_midi_event();
unsigned long get_next_midi_delay();
uint8_t get_next_byte(){
