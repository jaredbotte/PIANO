#include "sd_card.h"

void fatfs_init(){
    static FATFS fs;
    static DIR dir;
    static FILINFO fno;
    static FIL file;

    DSTATUS disk_state = STA_NOINIT;
    FRESULT ff_result;

    // Initialize FATFS disk I/0 interface

    static diskio_blkdev_t drives[] = 
    {
            DISKIO_BLOCKDEV_CONFIG(NRF_BLOCKDEV_BASE_ADDR(m_block_dev_sdc, block_dev), NULL)
    };

    diskio_blockdev_register(drives, ARRAY_SIZE(drives));

    for (uint32_t retries = 3; retries && disk_state; --retries)
    {
        disk_state = disk_initialize(0);
    }
    if (disk_state)
    {
        printf("Disk initialize Error!!!!!!\r\n");
        printf("Error type %d\r\n", ff_result);
        return;
    }

    uint32_t blocks_per_mb = (1024uL * 1024uL) / m_block_dev_sdc.block_dev.p_ops->geometry(&m_block_dev_sdc.block_dev)->blk_size;
    uint32_t capacity = m_block_dev_sdc.block_dev.p_ops->geometry(&m_block_dev_sdc.block_dev)->blk_count / blocks_per_mb;

    // Mount
    ff_result = f_mount(&fs, "", 1);
    if (ff_result)
    {
        printf("Mount failed\r\n");
        printf("Error type %d\r\n", ff_result);
    }

    // Listing directory
    ff_result = f_opendir(&dir, "/");

    if (ff_result) {
        printf("Directory listing failed!\r\n");
        printf("Error type %d\r\n", ff_result);
    }

    do {
        ff_result = f_readdir(&dir, &fno);
        
        if (ff_result != FR_OK) {
            printf("Directory reading failed!\r\n");
            printf("Error type %d\r\n", ff_result);
            return;
        }
    }while (fno.fname[0]);

    return;

}

void write_to_file (uint8_t* data, int size, char* filename) {
    FRESULT  ff_result;
    FIL      file;
    uint16_t bytes_written;

    //fatfs_init();

    ff_result = f_open(&file, filename, FA_READ | FA_WRITE | FA_OPEN_APPEND);
    if (ff_result != FR_OK) {
        printf("fread error func\r\n");
        printf("Error type %d\r\n", ff_result);
        return;
    }

    ff_result = f_write(&file, data, size, (UINT*)& bytes_written);

    if (ff_result != FR_OK) {
        printf("fwrite error func\r\n");
        printf("Error type %d\r\n", ff_result);
        return;
    }

    (void) f_close(&file);
}

// void fileWrite(void* p_event_data, uint16_t event_size) {
//     FRESULT  ff_result;
//     FIL      file;
//     uint16_t bytes_written;

//     //fatfs_init();
//     sd_write_evt* evt = (sd_write_evt*) p_event_data;

//     ff_result = f_open(&file, evt->filename, FA_READ | FA_WRITE | FA_OPEN_APPEND);
//     printf("FFRESULT: %d\r\n", ff_result);
//     if (ff_result != FR_OK) {
//         printf("fread error func\r\n");
//         printf("Error type %d\r\n", ff_result);
//         return;
//     }

//     ff_result = f_write(&file, evt->buf.data, evt->buf.length, (UINT*)& bytes_written);

//     if (ff_result != FR_OK) {
//         printf("fwrite error func\r\n");
//         printf("Error type %d\r\n", ff_result);
//         return;
//     }

//     (void) f_close(&file);
// }