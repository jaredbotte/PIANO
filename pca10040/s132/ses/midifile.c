#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "midifile.h"

uint8_t get_next_byte(){
    uint8_t byte = 0;
    UINT bytes_read = 0;
    FRESULT res = f_read(&midi_file.ptr, &byte, 1, &bytes_read);
    return byte;
}

MidiEvent get_midi_event(uint8_t newid){
    uint8_t note_info[2] = {0};
    UINT br = 0;
    FRESULT res = f_read(&midi_file.ptr, note_info, 2, &br);
    uint8_t newnote = note_info[0];
    uint8_t vel = note_info[1];
    printf("Creating MIDI Event with note %d, ID %d, and velocity %d\r\n", newnote, newid, vel);
    return (MidiEvent) {.ID = newid, .note=newnote, .velocity=vel};
}

void get_meta_event(){
    uint8_t meta_type = get_next_byte();
    unsigned long length = get_variable_data();
    if(meta_type == 0x2F){ // End of track
        // TODO: Change state of system! Song is over.
    } else if (meta_type == 0x51){ // Tempo setting
        // Length should be 3!
        uint8_t tempodat[3] = {0};
        UINT br;
        f_read(&midi_file.ptr, tempodat, length, &br);
        midi_file.tempo = (tempodat[0] << 16) | (tempodat[1] << 8) | tempodat[2]; 
        midi_file.mseconds_per_tick = (midi_file.tempo / midi_file.division) / 1000;
    } else {
        printf("Found meta event %X\r\n", meta_type);
        FRESULT res = f_lseek(&midi_file.ptr, length);
        printf("Seek result %d\r\n", res);
    }
}

unsigned long get_variable_data(){
    unsigned long v_data = 0;
    uint8_t more_bytes;
    do {
        v_data <<= 7;
        uint8_t next_byte = get_next_byte();
        more_bytes = next_byte & 0x80;
        v_data |= next_byte & 0x7f;
    } while(more_bytes);

    return v_data;
}

uint8_t read_next_track_event(){
    uint8_t evt = get_next_byte();
    if (evt == 0xFF){
        get_meta_event();
    } else if (evt == 0xF0){
        unsigned long len = get_variable_data();
        f_lseek(&midi_file.ptr, len + 1);
    } else if (evt == 0xF7){
        unsigned long len = get_variable_data();
        f_lseek(&midi_file.ptr, len);
    }
    return evt;
}

unsigned long read_next_midi_data(){
    MidiEvent updates[MIDI_EVENT_LIMIT];
    memset(updates, 0, MIDI_EVENT_LIMIT);
    int events_read = 0;
    unsigned long delay = 0;

    while(!delay && events_read < MIDI_EVENT_LIMIT && f_tell(&midi_file.ptr) < midi_file.next_track_start) {
        // TODO: grab the first delay and schedule this function
        // TODO: then, use the returned delay to schedule this function again.
        uint8_t evt = read_next_track_event();
        printf("found event: %X\r\n", evt);
        if (evt != 0xFF && evt != 0xF0 && evt != 0xF7) {
            MidiEvent mevt = get_midi_event(evt);
            printf("found Note");
            updates[events_read] = mevt;
            events_read++;
        }
        delay = get_variable_data();
    }
    if (f_tell(&midi_file.ptr) >= midi_file.next_track_start){
        printf("found next header");
        start_next_track();
    }
    // Update leds based on updates
    printf("events read: %d\r\n", events_read);
    // TODO: For some reason this for loop is still being entered.. why??
    for(int i = 0; i < events_read; i++){
        MidiEvent curr = updates[i];
        int stat = curr.ID == 0x90 ? 1 : 0;
        set_key_velocity(curr.note, stat, curr.velocity);
    }
    unsigned long delay_ms = delay * midi_file.mseconds_per_tick;
    return delay_ms; 
}

void start_next_track(){
    UINT br;
    uint8_t trkhead[8] = {0}; // 'MTrk' = 4, length = 4 | total 8
    FRESULT ff_result = f_read(&midi_file.ptr, trkhead, 8, &br);
    uint32_t trklen_bytes = (trkhead[4] << 24) | (trkhead[5] << 16) | (trkhead[6] << 8) | trkhead[7];
    midi_file.next_track_start = f_tell(&midi_file.ptr) + trklen_bytes;
}

unsigned long init_midi_file(char* filename){
   static FATFS fs;
    static DIR dir;
    static FILINFO fno;
    static FIL file;

    uint32_t bytes_written;
    FRESULT ff_result;
    DSTATUS disk_state = STA_NOINIT;

    // Initialize FATFS disk I/O interface by providing the block device. 
    static diskio_blkdev_t drives[] =
    {
            DISKIO_BLOCKDEV_CONFIG(NRF_BLOCKDEV_BASE_ADDR(m_block_dev_sdc, block_dev), NULL)
    };

    diskio_blockdev_register(drives, ARRAY_SIZE(drives));

    NRF_LOG_INFO("Initializing disk 0 (SDC)...");
    for (uint32_t retries = 3; retries && disk_state; --retries)
    {
        //printf("%x\n", disk_state);
        disk_state = disk_initialize(0);
    }
    if (disk_state)
 
    {
      
        NRF_LOG_INFO("Disk initialization failed.");
        return;
    }

    uint32_t blocks_per_mb = (1024uL * 1024uL) / m_block_dev_sdc.block_dev.p_ops->geometry(&m_block_dev_sdc.block_dev)->blk_size;
    uint32_t capacity = m_block_dev_sdc.block_dev.p_ops->geometry(&m_block_dev_sdc.block_dev)->blk_count / blocks_per_mb;
    NRF_LOG_INFO("Capacity: %d MB", capacity);

    NRF_LOG_INFO("Mounting volume...");
    ff_result = f_mount(&fs, "", 1);
    if (ff_result)
    {
        NRF_LOG_INFO("Mount failed.");
        return;
    }

    NRF_LOG_INFO("\r\n Listing directory: /");
    ff_result = f_opendir(&dir, "/");
    if (ff_result)
    {
        NRF_LOG_INFO("Directory listing failed!");
        return;
    }

    do
    {
        ff_result = f_readdir(&dir, &fno);
        if (ff_result != FR_OK)
        {
            NRF_LOG_INFO("Directory read failed.");
            return;
        }

        if (fno.fname[0])
        {
            if (fno.fattrib & AM_DIR)
            {
                NRF_LOG_RAW_INFO("   <DIR>   %s",(uint32_t)fno.fname);
            }
            else
            {
                NRF_LOG_RAW_INFO("%9lu  %s", fno.fsize, (uint32_t)fno.fname);
                printf("Filename: %s\r\n", (uint32_t)fno.fname);
            }
        }
    }
    while (fno.fname[0]);
    ff_result = f_open(&midi_file.ptr, "TEST.MID", FA_READ);
    if(ff_result != FR_OK){
    printf("%d\r\n", ff_result);
        return; // We've encountered an error
    }
    // Start by reading header
    uint8_t header[14] = {0}; // 'MThd' = 4, length = 4, format = 2, ntrks = 2, division = 2 | total = 14
    UINT br = 0;
    ff_result = f_read(&midi_file.ptr, header, 14, &br);
    printf("%d Read: %c%c %c%c\r\n", br, header[0], header[1], header[2], header[3]);
    if(ff_result != FR_OK){
      printf("%d\r\n", ff_result);
    }
    midi_file.format = header[8] << 8 | header[9];
    midi_file.numTracks = header[10] << 8 | header[11];
    midi_file.division = header[12] << 8 | header[13];
    midi_file.tempo = 500000; // 120 BPM
    midi_file.mseconds_per_tick = (midi_file.tempo / midi_file.division) / 1000;
    // Now we can start with the "meat" of the file
    start_next_track();
    unsigned long delay_ticks = get_variable_data();
    unsigned long delay_ms = delay_ticks * midi_file.mseconds_per_tick;
    printf("delaying for: %ld ms", delay_ms);
    return delay_ms;
    // NOTE: Check to see if fp is at next_track_start after getting delay (upon read_next_midi_data return)
}