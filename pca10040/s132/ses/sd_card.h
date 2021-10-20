#ifndef SD_CARD_H_
#define SD_CARD_H_

#include "ff.h"
#include "diskio_blkdev.h"
#include "bsp.h"
#include "nrf_block_dev_sdc.h"
#include "nrf_block_dev.h"

#define SDC_MISO_PIN 26 // DO
#define SDC_SCK_PIN  27 // SCK
#define SDC_MOSI_PIN 28 // DI
#define SDC_CS_PIN   29 // CS

#define SDC_BUFFER_SIZE 512


NRF_BLOCK_DEV_SDC_DEFINE(
        m_block_dev_sdc,
        NRF_BLOCK_DEV_SDC_CONFIG(
                SDC_SECTOR_SIZE,
                APP_SDCARD_CONFIG(SDC_MOSI_PIN, SDC_MISO_PIN, SDC_SCK_PIN, SDC_CS_PIN)
        ),
        NFR_BLOCK_DEV_INFO_CONFIG("Nordic", "SDC", "1.00")
);

typedef struct _sd_write_buffer {
        uint16_t length;
        char data[SDC_BUFFER_SIZE];
} sd_write_buffer;

typedef struct _sd_write_evt {
        sd_write_buffer buf;
        char* filename;
}sd_write_evt;

void fileWrite(void* p_event_data, uint16_t event_size);

void fatfs_init ();

void write_to_file (uint8_t* data, int size, char* filename);


#endif