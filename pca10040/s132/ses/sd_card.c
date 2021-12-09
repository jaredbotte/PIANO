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
        print_error(ff_result);
        return;
    }

    uint32_t blocks_per_mb = (1024uL * 1024uL) / m_block_dev_sdc.block_dev.p_ops->geometry(&m_block_dev_sdc.block_dev)->blk_size;
    uint32_t capacity = m_block_dev_sdc.block_dev.p_ops->geometry(&m_block_dev_sdc.block_dev)->blk_count / blocks_per_mb;

    // Mount
    ff_result = f_mount(&fs, "", 1);
    if (ff_result)
    {
        printf("Mount failed\r\n");
        print_error(ff_result);
    }

    // Listing directory
    ff_result = f_opendir(&dir, "/");

    if (ff_result) {
        printf("Directory listing failed!\r\n");
        print_error(ff_result);
    }

    do {
        ff_result = f_readdir(&dir, &fno);
        
        if (ff_result != FR_OK) {
            printf("Directory reading failed!\r\n");
            print_error(ff_result);
            return;
        }
    }while (fno.fname[0]);

    return;

}

bool checkSD() {
    static FATFS fs;
    static DIR dir;
    static FILINFO fno;
    static FIL file;

    FRESULT ff_result;
    ff_result = f_mount(&fs, "", 1);
    if (ff_result)
    {
        printf("Mount failed\r\n");
        print_error(ff_result);

        return false;
    }

    // Listing directory
    ff_result = f_opendir(&dir, "/");

    if (ff_result) {
        printf("Directory listing failed!\r\n");
        print_error(ff_result);
        return false;
    }

    do {
        ff_result = f_readdir(&dir, &fno);
        
        if (ff_result != FR_OK) {
            printf("Directory reading failed!\r\n");
            print_error(ff_result);
            return false;
        }
    }while (fno.fname[0]);

    return true;

}

void print_error(FRESULT fr)
{
    const char *errs[] = {
            [FR_OK] = "Success",
            [FR_DISK_ERR] = "Hard error in low-level disk I/O layer",
            [FR_INT_ERR] = "Assertion failed",
            [FR_NOT_READY] = "Physical drive cannot work",
            [FR_NO_FILE] = "File not found",
            [FR_NO_PATH] = "Path not found",
            [FR_INVALID_NAME] = "Path name format invalid",
            [FR_DENIED] = "Permision denied",
            [FR_EXIST] = "Prohibited access",
            [FR_INVALID_OBJECT] = "File or directory object invalid",
            [FR_WRITE_PROTECTED] = "Physical drive is write-protected",
            [FR_INVALID_DRIVE] = "Logical drive number is invalid",
            [FR_NOT_ENABLED] = "Volume has no work area",
            [FR_NO_FILESYSTEM] = "Not a valid FAT volume",
            [FR_MKFS_ABORTED] = "f_mkfs aborted",
            [FR_TIMEOUT] = "Unable to obtain grant for object",
            [FR_LOCKED] = "File locked",
            [FR_NOT_ENOUGH_CORE] = "File name is too large",
            [FR_TOO_MANY_OPEN_FILES] = "Too many open files",
            [FR_INVALID_PARAMETER] = "Invalid parameter",
    };
    if (fr < 0 || fr >= sizeof errs / sizeof errs[0])
        printf("Invalid error\r\n");
    else
        printf("%s\r\n", errs[fr]);
}

void write_to_file (uint8_t* data, int size, char* filename) {
    FRESULT  ff_result;
    FIL      file;
    uint16_t bytes_written;

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