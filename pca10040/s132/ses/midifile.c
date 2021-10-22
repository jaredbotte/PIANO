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

MidiEvent get_midi_event(const uint8_t stat){
    // NOTE: For whatever reason, stat is being overwritten here
    // NOTE: This is a MAJOR ISSUE!
    // NOTE: Also, the "MidiEvent m = " line does not ever get run...
    uint8_t note_info[2] = {0};
    UINT br;
    FRESULT res = f_read(&midi_file.ptr, note_info, 2, &br);
    uint8_t newnote = note_info[0];
    uint8_t vel = note_info[1];
    //printf("Creating MIDI Event with note %d, ID %d, and velocity %d\r\n", newnote, *wtf, vel);
    MidiEvent m = (MidiEvent) {.ID = stat, .note=newnote, .velocity=vel};
    return m;
}

void get_meta_event(){
    uint8_t meta_type = get_next_byte();
    unsigned long length = get_variable_data();
    if(meta_type == 0x2F){ // End of track
        if (f_tell(&midi_file.ptr) + 1 == f_size(&midi_file.ptr)){
            printf("EOF");
            while(true); // TODO: Obviously, fix this line.
        }
        start_next_track();
    } else if (meta_type == 0x51){ // Tempo setting
        //printf("tempo!\r\n");
        // Length should be 3!
        uint8_t tempodat[3] = {0};
        UINT br;
        f_read(&midi_file.ptr, tempodat, length, &br);
        midi_file.tempo = (tempodat[0] << 16) | (tempodat[1] << 8) | tempodat[2]; 
        midi_file.tempo /= TEMP_DIV;
        midi_file.mseconds_per_tick = (midi_file.tempo / midi_file.division) / 1000;
    } else {
        printf("skipping meta event %X with length %X\r\n", meta_type, length);
        FRESULT res = f_lseek(&midi_file.ptr, f_tell(&midi_file.ptr) + length);
        if(res != FR_OK){
            printf("NOPE NOPE NOPE NOPE NOPE");
        }
    }
    printf("FP: %X", f_tell(&midi_file.ptr));
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
    //printf("Found track event %X\n\r", evt);
    if (evt == 0xFF){
        get_meta_event();
    } else if (evt == 0xF0){
        //printf("Found some text.\r\n");
        unsigned long len = get_variable_data();
        f_lseek(&midi_file.ptr, f_tell(&midi_file.ptr) + len + 1);
    } else if (evt == 0xF7){
        //printf("Found some stuff. Skipping it.\r\n");
        unsigned long len = get_variable_data();
        f_lseek(&midi_file.ptr, f_tell(&midi_file.ptr) + len);
    } else if (evt == 0xB0){
        //printf("Found B0\r\n");
        f_lseek(&midi_file.ptr, f_tell(&midi_file.ptr) + 2);
    } else if ((evt & 0xF0) == 0xC0) {
        //printf("MIDI track event.\r\n");
        uint8_t trash = get_next_byte(); // No clue what this is but causes mass chaos if not removed.    
    } else {
        UINT loc = f_tell(&midi_file.ptr);
        printf("WARNING: EVENT @ %d %X NOT KNOWN!\r\n", loc, evt);
    }
    return evt;
}

unsigned long read_next_midi_data(){
    MidiEvent updates[MIDI_EVENT_LIMIT];
    memset(updates, 0, MIDI_EVENT_LIMIT);
    int events_read = 0;
    unsigned long delay = 0;
    //printf("Reading next midi data.\r\n");
    while(!delay && events_read < MIDI_EVENT_LIMIT) {
        uint8_t evt = (read_next_track_event());
        if ((evt & 0xF0) == 0x90 || (evt & 0xF0) == 0x80) {
            MidiEvent mevt = get_midi_event(evt);
            updates[events_read] = mevt;
            events_read++;
        }
        delay = get_variable_data();
    }
    // Update leds based on updates
    //printf("MIDI events read: %d\r\n", events_read);
    // TODO: For some reason this for loop is still being entered.. why??
    for(int i = 0; i < events_read; i++){
        MidiEvent curr = updates[i];
        int stat = curr.ID == 0x90 ? 1 : 0;
        //printf("Setting key %d to %d\r\n", curr.note, stat);
        set_key_velocity(curr.note, stat, curr.velocity);
    }
    unsigned long delay_ms = delay * midi_file.mseconds_per_tick;
    return delay_ms; 
}

void start_next_track(){
    UINT br;
    // TODO: See if we're at the end of the file, if so do something about it.
    uint8_t trkhead[8] = {0}; // 'MTrk' = 4, length = 4 | total 8
    FRESULT ff_result = f_read(&midi_file.ptr, trkhead, 8, &br);
    printf("Read track header. Start: %X End: %X\r\n", trkhead[0], trkhead[7]);
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
    printf("Parsed Header\r\n");
    if(ff_result != FR_OK){
      printf("%d\r\n", ff_result);
    }
    midi_file.format = header[8] << 8 | header[9];
    midi_file.numTracks = header[10] << 8 | header[11];
    midi_file.division = header[12] << 8 | header[13];
    midi_file.tempo = 500000 / TEMP_DIV; // 120 BPM
    midi_file.mseconds_per_tick = (midi_file.tempo / midi_file.division) / 1000;
    // Now we can start with the "meat" of the file
    start_next_track();
    unsigned long delay_ticks = get_variable_data();
    unsigned long delay_ms = delay_ticks * midi_file.mseconds_per_tick;
    printf("delaying for: %ld ms\r\n", delay_ms);
    return delay_ms;
}