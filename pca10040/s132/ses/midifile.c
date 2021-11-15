#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "midifile.h"
#include "nrf_delay.h"

bool endFlag = false;
typedef enum states{VIS, LTP, PA}Mode;
float tempoDiv = 1.0;
extern currentMode;
extern Key* key_array;
uint8_t last_event;

uint8_t get_next_byte(){
    uint8_t byte = 0;
    UINT bytes_read = 0;
    FRESULT res = f_read(&midi_file.ptr, &byte, 1, &bytes_read);
    return byte;
}

MidiEvent get_midi_event(uint8_t temp){
    uint8_t stat = temp;
    uint8_t note_info[2] = {0};
    UINT br = 0;
    FRESULT res = f_read(&midi_file.ptr, note_info, 2, &br);
    uint8_t newnote = note_info[0] - 21;
    uint8_t vel = note_info[1];
    return (MidiEvent) {.ID = stat, .note=newnote, .velocity=vel};
}

void setTempoDiv(float div){
    if(div != 0){
        tempoDiv = div;
        printf("Set tempoDiv to %f\r\n", div);
    }
}

void get_meta_event(){
    uint8_t meta_type = get_next_byte();
    unsigned long length = get_variable_data();
    //printf("Length rcvd: %x\r\n", length);
    if(meta_type == 0x2F){ // End of track
        if (f_tell(&midi_file.ptr) < f_size(&midi_file.ptr)) {
          start_next_track();
        }   
        else {
          printf("EOF reached\r\n");
          currentMode = VIS;
          endFlag = true;
          f_close(&midi_file.ptr);
        }   
    } else if (meta_type == 0x51){ // Tempo setting
        //printf("tempo!\r\n");
        // Length should be 3!
        uint8_t tempodat[3] = {0};
        UINT br;
        f_read(&midi_file.ptr, tempodat, length, &br);
        midi_file.tempo = (tempodat[0] << 16) | (tempodat[1] << 8) | tempodat[2]; 
        midi_file.tempo /= tempoDiv;
        midi_file.mseconds_per_tick = (midi_file.tempo / midi_file.division) / 1000;
        //printf("ms/tick: %f\r\n", midi_file.mseconds_per_tick);
    }else {
        //printf("skipping meta event %X with length %X\r\n", meta_type, length);
        FRESULT res = f_lseek(&midi_file.ptr, f_tell(&midi_file.ptr) + length);
    }
    //printf("FP: %X", f_tell(&midi_file.ptr));
}

unsigned long get_variable_data()
{
    unsigned long value;
    unsigned char c;

    if ( (value = get_next_byte()) & 0x80 )
    {
       value &= 0x7F;
       do
       {
         value = (value << 7) + ((c = get_next_byte()) & 0x7F);
       } while (c & 0x80);
    }
    //printf("%ld\r\n", value);
    return value;
}

uint8_t read_next_track_event(){
    // NOTE: This function will keep reading bytes until it finds something it knows.
    // DISCLAIMER: This will not parse running events properly. All events after first set will be discarded.
    
    uint8_t evt;
    bool runon = false;
    bool got_f = false;
    uint8_t last_valid_event;
    unsigned long len;

    do{
        evt = get_next_byte();
        got_f = false;
        runon = false;

        switch(evt){
            case 0xFF:
                get_meta_event();
                got_f = true;
                break;
            case 0xF0:
                len = get_variable_data();
                f_lseek(&midi_file.ptr, f_tell(&midi_file.ptr) + len);
                got_f = true;
                break;
             case 0xF7:
                len = get_variable_data();
                f_lseek(&midi_file.ptr, f_tell(&midi_file.ptr) + len);
                got_f = true;
                break;
        }

        if(!got_f){
            switch(evt & 0xF0){
                case 0xA0:
                case 0xB0:
                case 0xE0:
                    f_lseek(&midi_file.ptr, f_tell(&midi_file.ptr) + 2);
                    break;
                case 0xC0:
                case 0xD0:
                    f_lseek(&midi_file.ptr, f_tell(&midi_file.ptr) + 1);
                    break;
                case 0x90:
                case 0x80:
                    break;
                default:
                    runon = true;
            }
        }
    }while(runon);
    return evt;
} 

unsigned long read_next_midi_data(){
    unsigned long delay = 0;
    while(!endFlag && delay == 0) {
        uint8_t evt = (read_next_track_event());
        uint8_t chan = evt & 0xF;
        evt &= 0xF0;
        if ((evt == 0x90 || evt == 0x80) && chan != 0xA) {
            MidiEvent mevt = get_midi_event(evt);
            if (evt == 0x90) {
                set_key(mevt.note, true, true, mevt.velocity, LEARN_COLOR);
            } else {
                set_key(mevt.note, false, true, mevt.velocity, OFF);
            }
        }
        delay = get_variable_data();
    }

    if (endFlag) {
      return -1;
    }

    double delay_ms = delay * midi_file.mseconds_per_tick;
    unsigned long delay_ms_int= (unsigned long)delay_ms;
    //printf("Delaying %ld ms\r\n", delay_ms_int);

    // To clear the IDC, IXC, UFC, OFC, DZC, and IOC flags, use 0x0000009F mask on FPSCR register
    uint32_t fpscr_reg = __get_FPSCR();
    __set_FPSCR(fpscr_reg & ~(0x0000009F));
    (void) __get_FPSCR();
    // Clear the pending FPU interrupt. Necessary when the application uses a SoftDevice with sleep modes
    NVIC_ClearPendingIRQ(FPU_IRQn);

    return delay_ms_int;
}


static void correct_delay_handler(void* p_context){
    MidiEvent* mevt = (MidiEvent*) p_context;
    set_key(mevt->note, true, true, mevt->velocity, LEARN_COLOR);
}


void learn_next_midi_data(){
    unsigned long delay = 0;
    while(!endFlag && delay == 0) {
        uint8_t evt = (read_next_track_event());
        uint8_t chan = evt & 0xF;
        evt &= 0xF0;
        if ((evt == 0x90 || evt == 0x80) && chan != 0xA) {
            MidiEvent mevt = get_midi_event(evt);
            if(evt == 0x90){
                /*TODO make this stay green for ~ half a second before turning back to the learn color if the key is held down
                if(key_array[mevt.note].userLit){
                    //set_key(mevt.note, true, true, mevt.velocity, LEARN_COLOR);
                    APP_TIMER_DEF(correct_delay);
                    ret_code_t err_code;
                    err_code = app_timer_create(&correct_delay, APP_TIMER_MODE_SINGLE_SHOT, correct_delay_handler);
                    APP_ERROR_CHECK(err_code);

                    err_code = app_timer_start(correct_delay, APP_TIMER_TICKS(4), &mevt);
                    APP_ERROR_CHECK(err_code);
                }
                */
                set_key(mevt.note, true, true, mevt.velocity, LEARN_COLOR);
            } 
            else {
                set_key(mevt.note, false, true, mevt.velocity, OFF);
            }
        }
        delay = get_variable_data();
        //f_lseek(&midi_file.ptr, f_tell(&midi_file.ptr) + get_variable_data());
    }
    
    if (endFlag) {
      //return -1;
      // TODO: Pass this information along?
    }
}

void start_next_track(){
    UINT br;
    // TODO: See if we're at the end of the file, if so do something about it.
    uint8_t trkhead[8] = {0}; // 'MTrk' = 4, length = 4 | total 8
    FRESULT ff_result = f_read(&midi_file.ptr, trkhead, 8, &br);
    printf("Read track header. Start: %X End: %X\r\n", trkhead[0], trkhead[7]);
    printf("File ptr loc: %x\r\n",f_tell(&midi_file.ptr));
}

unsigned long init_midi_file(char* filename){
    static FATFS fs;
    static DIR dir;
    static FILINFO fno;
    static FIL file;

    uint32_t bytes_written;
    FRESULT ff_result;
    DSTATUS disk_state = STA_NOINIT;

    ff_result = f_mount(&fs, "", 1);
    if (ff_result)
    {
            return;
    }
    ff_result = f_opendir(&dir, "/");
    if (ff_result)
    {
        return;
    }

    do
    {
        ff_result = f_readdir(&dir, &fno);
        if (ff_result != FR_OK)
        {
            return;
        }
    }
    while (fno.fname[0]);

    endFlag = false;
    //TODO Check long file name
    char fn[32];
    strcpy(fn, filename);
    ff_result = f_open(&midi_file.ptr, fn, FA_READ);

    //ff_result = f_open(&midi_file.ptr, "TEST.MID", FA_READ);
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
    midi_file.tempo = 500000 * tempoDiv; // 120 BPM
    midi_file.mseconds_per_tick = (midi_file.tempo / midi_file.division) / 1000.;
    printf("ms/tick: %f\r\n", midi_file.mseconds_per_tick);
    //printf("Tempo: %ld\r\n",  midi_file.tempo);
   // printf("Division: %ld\r\n",  midi_file.division);
    
    // Now we can start with the "meat" of the file
    start_next_track();
    unsigned long delay_ticks = get_variable_data();
    double delay_ms = delay_ticks * midi_file.mseconds_per_tick;
    
    unsigned long delay_ms_int = (unsigned long)delay_ms;
    printf("Initial Delay: %ld ms\r\n", delay_ms_int);
    // To clear the IDC, IXC, UFC, OFC, DZC, and IOC flags, use 0x0000009F mask on FPSCR register
    uint32_t fpscr_reg = __get_FPSCR();
    __set_FPSCR(fpscr_reg & ~(0x0000009F));
    (void) __get_FPSCR();
    // Clear the pending FPU interrupt. Necessary when the application uses a SoftDevice with sleep modes
    NVIC_ClearPendingIRQ(FPU_IRQn);

    return delay_ms_int;
}