#pragma once
#include "diskio_blkdev.h"
#include "nrf_block_dev_sdc.h"
#include "sd_card.h"
#include "nrf_log.h"
#include "ff.h"
#include "leds.h"

#define MIDI_EVENT_LIMIT 10

typedef struct {
    uint8_t ID;
    uint8_t note;
    uint8_t velocity;
} MidiEvent;

typedef struct {
    FIL ptr;
    uint16_t format;
    uint16_t numTracks;
    uint16_t division;
    int mseconds_per_tick;
    uint32_t tempo; // uS/qn
    uint64_t next_track_start;
} MidiFile;

static MidiFile midi_file;


unsigned long read_next_midi_data();
MidiEvent get_midi_event(uint8_t newid);
void get_meta_event();
unsigned long get_variable_data();
uint8_t get_next_byte();
unsigned long init_midi_file(char* filename);
void start_next_track();
