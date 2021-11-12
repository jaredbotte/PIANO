#pragma once
#include "diskio_blkdev.h"
#include "nrf_block_dev_sdc.h"
#include "sd_card.h"
#include "nrf_log.h"
#include "ff.h"
#include "leds.h"
#include "arm_math.h"

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
    double mseconds_per_tick;
    float tempo; // uS/qn
} MidiFile;

static MidiFile midi_file;


unsigned long read_next_midi_data();
void learn_next_midi_data(int* numKeys);
void addIncorrect();
MidiEvent get_midi_event(uint8_t stat);
void get_meta_event();
unsigned long get_variable_data();
uint8_t get_next_byte();
unsigned long init_midi_file(char* filename);
void start_next_track();
void setTempoDiv(float div);
