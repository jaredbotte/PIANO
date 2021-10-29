/** @file
 *
 * @defgroup ble_sdk_uart_over_ble_main main.c
 * @{
 * @ingroup  ble_sdk_app_nus_eval
 * @brief    UART over BLE application main file.
 *
 * This file contains the source code for a sample application that uses the Nordic UART service.
 * This application uses the @ref srvlib_conn_params module.
 */

#include <stdbool.h> // Needed for UART
#include <stdint.h>
#include <string.h>
#include "nordic_common.h" // ble
#include "nrf.h" // both
#include "bsp.h" // fatfs
#include "ff.h" // fatfs
#include "diskio_blkdev.h" // fatfs
#include "nrf_block_dev_sdc.h" // fatfs
#include "ble_hci.h" // ble
#include "ble_advdata.h" // ble
#include "ble_advertising.h"// ble
#include "ble_conn_params.h" // ble
#include "nrf_sdh.h" // ble
#include "nrf_sdh_soc.h" // ble
#include "nrf_sdh_ble.h" // ble
#include "nrf_ble_gatt.h" // ble
#include "nrf_ble_qwr.h" // ble
#include "app_timer.h" // ble
#include "ble_nus.h" // ble
#include "app_uart.h" // ble
#include "app_util_platform.h" // ble
#include "bsp_btn_ble.h" // ble
#include "nrf_pwr_mgmt.h" // ble
#include "leds.h" // leds
#include "nrf_delay.h" // leds, uart
#include "app_error.h" //uart
#include "sd_card.h"
#include "app_scheduler.h"
#include "midifile.h"


// BLE
#if defined (UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined (UARTE_PRESENT)
#include "nrf_uarte.h"
#endif

// fatfs
#define FILE_NAME "Test with bluetooth.txt"
#define TEST_STRING "This is a test string."

// BOTH
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

// BLE
#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */

#define DEVICE_NAME                     "Nordic_UART"                               /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#define APP_ADV_INTERVAL                64                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */

#define APP_ADV_DURATION                0                                           /**< The advertising duration (180 seconds) in units of 10 milliseconds. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define UART_TX_BUF_SIZE                256                                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                256                                         /**< UART RX buffer size. */

#define BLE_BUF_SIZE                    256

#define SCHED_MAX_EVENT_DATA_SIZE       sizeof(sd_write_evt)
#define SCHED_QUEUE_SIZE                10

bool colorChanged   = false;
bool rChanged       = false;
bool gChanged       = false;
bool bChanged       = false;
bool writeEnable    = false;
Color led_color     = {.red = 0, .green = 0, .blue = 0};

volatile bool fileTransfer   = false;
volatile bool bufferFilled   = false;
volatile bool transferStarted = false;
char filename[64];

bool bufferBusy = false;

bool bufferLock = false;

int bytes_w;
int bytes_s;
bytes_s = 0;
bytes_w = 0;


sd_write_buffer sd_data_buffer = {.length = 0, .data = {0x00}};

int num_received;
int num_written;

num_received = 0;
num_written  = 0;

bool transfer_over = false;


BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

sd_write_evt sd_evt;


// sdcard code
//NRF_BLOCK_DEV_SDC_DEFINE(
//        m_block_dev_sdc,
//        NRF_BLOCK_DEV_SDC_CONFIG(
//                SDC_SECTOR_SIZE,
//                APP_SDCARD_CONFIG(SDC_MOSI_PIN, SDC_MISO_PIN, SDC_SCK_PIN, SDC_CS_PIN)
//        ),
//        NFR_BLOCK_DEV_INFO_CONFIG("Nordic", "SDC", "1.00")
//);

/**
 * @brief Function for demonstrating FAFTS usage.
 */
static void fatfs_example()
{
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
            }
        }
    }
    while (fno.fname[0]);
    NRF_LOG_RAW_INFO("");

    NRF_LOG_INFO("Writing to file " FILE_NAME "...");
    ff_result = f_open(&file, FILE_NAME, FA_READ | FA_WRITE | FA_OPEN_APPEND);
    if (ff_result != FR_OK)
    {
        NRF_LOG_INFO("Unable to open or create file: " FILE_NAME ".");
        return;
    }

    ff_result = f_write(&file, TEST_STRING, sizeof(TEST_STRING) - 1, (UINT *) &bytes_written);
    if (ff_result != FR_OK)
    {
       printf("Write failed\r\n.");
    }
    else
    {
        NRF_LOG_INFO("%d bytes written.", bytes_written);
    }

    (void) f_close(&file);
    return;
}

void led_update (void* p_event_data, uint16_t event_size) {
  update_led_strip();
}

void TIMER3_IRQHandler(void)
{
  NRF_TIMER3->EVENTS_COMPARE[0] = 0;           //Clear compare register 0 event	
  //update_led_strip();
  app_sched_event_put(&sd_evt, sizeof(sd_evt), led_update);
}


void fileWrite(void* p_event_data, uint16_t event_size) {
    // FRESULT  ff_result;
    // FIL      file;
    // uint16_t bytes_written;

    // //fatfs_init();
    // sd_write_evt* evt = (sd_write_evt*) p_event_data;
    // printf("Data: %s\r\n", evt->buf.data);
    // ff_result = f_open(&file, evt->filename, FA_READ | FA_WRITE | FA_OPEN_APPEND);
    // printf("FFRESULT: %d\r\n", ff_result);
    // if (ff_result != FR_OK) {
    //     printf("fread error func\r\n");
    //     printf("Error type %d\r\n", ff_result);
    //     return;
    // }

    // ff_result = f_write(&file, evt->buf.data, evt->buf.length, (UINT*)& bytes_written);

    // if (ff_result != FR_OK) {
    //     printf("fwrite error func\r\n");
    //     printf("Error type %d\r\n", ff_result);
    //     return;
    // }

    // (void) f_close(&file);
    //fatfs_example();

    static FATFS fs;
    static DIR dir;
    static FILINFO fno;
    static FIL file;

    uint32_t bytes_written;
    FRESULT ff_result;
    DSTATUS disk_state = STA_NOINIT;
    sd_write_evt* evt = (sd_write_evt*) p_event_data;

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
      
        printf("Disk initialization failed.\r\t");
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
            }
        }
    }
    while (fno.fname[0]);
    NRF_LOG_RAW_INFO("");

    NRF_LOG_INFO("Writing to file " FILE_NAME "...");
    char fn[32];
    strcpy(fn, evt->filename);

    ff_result = f_open(&file,fn, FA_READ | FA_WRITE | FA_OPEN_APPEND);
    if (ff_result != FR_OK)
    {
        NRF_LOG_INFO("Unable to open or create file: " FILE_NAME ".");
        printf("File open failed!!!!! Error type %d\r\n", ff_result);
        return;
    }

    ff_result = f_write(&file, evt->buf.data, evt->buf.length, (UINT *) &bytes_written);
    if (ff_result != FR_OK)
    {
       printf("Write failed\r\n.");
    }
    else
    {
        NRF_LOG_INFO("%d bytes written.", bytes_written);
        num_written++;
        bytes_w+=bytes_written;

        printf("%d bytes has been written\r\n", bytes_w);
    }

    (void) f_close(&file);
    return;
}

/**@brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Function for initializing the timer module.
 */
static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_evt       Nordic UART Service event.
 */
/**@snippet [Handling the data received over BLE] */
static void nus_data_handler(ble_nus_evt_t * p_evt)
{

    if (p_evt->type == BLE_NUS_EVT_RX_DATA)
    {
        uint32_t err_code;
        
        NRF_LOG_DEBUG("Received data from BLE NUS. Writing data on UART.");
        NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);

        uint8_t data[BLE_BUF_SIZE];
        memset(&data, 0,BLE_BUF_SIZE);

        for (uint32_t i = 0; i < p_evt->params.rx_data.length; i++) {
            data[i] = p_evt->params.rx_data.p_data[i];
            sd_evt.buf.data[i] = p_evt->params.rx_data.p_data[i];
        }
        int size = p_evt->params.rx_data.length;
        sd_evt.buf.length = p_evt->params.rx_data.length;

        if (strncmp(&data, "write", 5) == 0){
            //writeEnable = true;
            sd_evt.filename = "test.txt";
            app_sched_event_put(&sd_evt, sizeof(sd_evt), fileWrite);
        }

        if (strncmp(&data, "blue",4) == 0) {
            Color blue = {.red = 0, .green = 0, .blue = 60};
            fill_color(blue);
            //err_code = ble_nus_data_send(&m_nus, "Turned_on", 9, m_conn_handle);
        }

        if (strncmp(&data, "red",3) == 0) {
            Color red = {.red = 60, .green = 0, .blue = 0};
            fill_color(red);
            //err_code = ble_nus_data_send(&m_nus, "Turned_on", 9, m_conn_handle);
        }

        if (strncmp(&data, "green",5) == 0) {
            Color green = {.red = 0, .green = 60, .blue = 0};
            fill_color(green);
            //err_code = ble_nus_data_send(&m_nus, "Turned_on", 9, m_conn_handle);
        }

        if (strncmp(&data, "off",3) == 0) {
            Color off = {.red = 0, .green = 0, .blue = 0};
            fill_color(off);
            //err_code = ble_nus_data_send(&m_nus, "Turned_on", 9, m_conn_handle);

        }

        if (strncmp(&data, "Color_Changed",13) == 0) {
            colorChanged = true;
            rChanged == false;
            gChanged == false;
            bChanged == false;

            return;
        }

        if ((colorChanged == true) && (rChanged == false) && (gChanged == false) && (bChanged == false)) {
            if (strncmp(&data, "red",3) == 0) {
                rChanged = true;
            }
            else if (strncmp(&data, "green",5) == 0) {
                gChanged = true;
            }
            else if (strncmp(&data, "blue",4) == 0) {
                bChanged = true;
            }

            return;
        }

        if ((colorChanged == true) && (rChanged == true) && (gChanged == false) && (bChanged == false) && (strncmp(&data, "red",3) != 0)) {
            int color_value = atoi(&data);
            led_color.red = color_value;
            //printf("Red Color Val: %d\r\n",color_value);

            fill_color(led_color);
            colorChanged = false;
            rChanged = false;

            //return;
        }
        else if ((colorChanged == true) && (rChanged == false) && (gChanged == true) && (bChanged == false) && (strncmp(&data, "green",5) != 0)) {
            int color_value = atoi(&data);
            led_color.green = color_value;
            //printf("Green Color Val: %d\r\n",color_value);

            fill_color(led_color);
            colorChanged = false;
            gChanged = false;

            //return;
        }
        else if ((colorChanged == true) && (rChanged == false) && (gChanged == false) && (bChanged == true) && (strncmp(&data, "blue",4) != 0)) {
            int color_value = atoi(&data);
            led_color.blue = color_value;
            //printf("Blue Color Val: %d\r\n",color_value);

            fill_color(led_color);
            colorChanged = false;
            bChanged = false;

            //return;
        }


        // File transfer

        if (strncmp(&data, "EOF",3) == 0) {
            fileTransfer = false;
            transferStarted = false;
            printf("\r\nTransfer Completed\r\n");
            transfer_over = true;
            printf("\r\nNumber of reception: %d\r\n", num_received);
            printf("Number of write: %d\r\n", num_written);
            printf("Filename: %s\r\n", filename);
            memset(filename,'\0', sizeof(filename));
            return;
        }
        
        if (strncmp(&data, "File Transfer",13) == 0) {
            fileTransfer = true;
            return;
        }

        if (fileTransfer && !transferStarted) {// seting filename
            strncpy(&filename, &data, size);
            transferStarted = true;
            //printf("File Name: %s\r\n", filename);
            //printf("Filename size: %d\r\n", strlen(filename));
            //printf("Size: %d\r\n", size);
            filename[strlen(filename)-1] = '\0';
            return;
        }

        if (fileTransfer && transferStarted) {//writing data

            //while (bufferLock){};
            
                bufferBusy = true;
                for (uint32_t i = 0; i < size; i++) {
                    sd_data_buffer.data[i] = data[i];
                }
                sd_data_buffer.length = size;
                num_received++;
                bufferFilled = true;
                bufferBusy   = false;
                
                //strcpy(&(sd_evt.filename), &filename);
                //printf("Filename: %s\r\n", sd_evt.filename);
                sd_evt.filename = "test3.mid";
                app_sched_event_put(&sd_evt, sizeof(sd_evt), fileWrite);
                
                return;
            
        }

        




        
        //  for (uint32_t i = 0; i < p_evt->params.rx_data.length; i++)
        //  {


        //      do
        //      {
        //          err_code = app_uart_put(p_evt->params.rx_data.p_data[i]);
        //          if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_BUSY))
        //          {
        //              NRF_LOG_ERROR("Failed receiving NUS message. Error 0x%x. ", err_code);
        //              APP_ERROR_CHECK(err_code);
        //          }
        //      } while (err_code == NRF_ERROR_BUSY);
        //  }
        //  if (p_evt->params.rx_data.p_data[p_evt->params.rx_data.length - 1] == '\r')
        //  {
        //      while (app_uart_put('\n') == NRF_ERROR_BUSY);
        //  }
    }

}
/**@snippet [Handling the data received over BLE] */

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t           err_code;
    ble_nus_init_t     nus_init;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            sleep_mode_enter();
            break;
        default:
            break;
    }
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected");
            // LED indication will be changed when advertising starts.
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}


/**@brief Function for initializing the GATT library. */
void gatt_init(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
void bsp_event_handler(bsp_event_t event)
{
    uint32_t err_code;
    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break;

        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_WHITELIST_OFF:
            if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
            {
                err_code = ble_advertising_restart_without_whitelist(&m_advertising);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
            }
            break;

        default:
            break;
    }
}


/**@brief   Function for handling app_uart events.
 *
 * @details This function will receive a single character from the app_uart module and append it to
 *          a string. The string will be be sent over BLE when the last character received was a
 *          'new line' '\n' (hex 0x0A) or if the string has reached the maximum data length.
 */
/**@snippet [Handling the data received over UART] */
static int noteFlag = 0; //Var that controls MIDI-DIN reception.
static uint8_t lastEvent = 0;
uint8_t keyNum = 0x00;
//uint64_t noteListLower = 0;
//uint32_t noteListUpper = 0;
void uart_event_handle(app_uart_evt_t * p_event)
{
    //app_util_critical_region_enter(&nest);
    static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
    static uint8_t index = 0;
    uint32_t       err_code;
    uint8_t eventUART = 0x00;

    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:  
            while(app_uart_get(&eventUART) != NRF_SUCCESS); //Pull a byte off the FIFO
            if(noteFlag == 0 && (eventUART == 0x90 || eventUART == 0x80)) { //Event headder
              //noteFlag = eventUART == 0x90 ? 1 : 2;
              noteFlag = 1;
              lastEvent = eventUART;
              bsp_board_led_invert(2);
            }
            else if(noteFlag == 1) //Note info && (eventUART < 128) && (eventUART >= 0)
            {
              keyNum = eventUART - 21;
              noteFlag = 2;
            }
            else if(noteFlag == 2) //Velocity info
            {
              int type = lastEvent == 0x90 ? 1 : 0; 
              //set_key(keyNum, type, GREEN);
              set_key_velocity(keyNum, type, eventUART);
              //eventUART is now the velocity.
              noteFlag = 0;
            }
            else
            {
              //UNUSED_VARIABLE(app_uart_get(&data_array[index]));
              data_array[index] = eventUART; //Give the data back to BLE if it was not a MIDI event.
              index++;

              if ((data_array[index - 1] == '\n') ||
                  (data_array[index - 1] == '\r') ||
                  (index >= m_ble_nus_max_data_len))
              {
                  if (index > 1)
                  {
                      //NRF_LOG_DEBUG("Ready to send data over BLE NUS");
                      //NRF_LOG_HEXDUMP_DEBUG(data_array, index);

                      do
                      {
                          uint16_t length = (uint16_t)index;
                          err_code = ble_nus_data_send(&m_nus, data_array, &length, m_conn_handle);
                          if ((err_code != NRF_ERROR_INVALID_STATE) &&
                              (err_code != NRF_ERROR_RESOURCES) &&
                              (err_code != NRF_ERROR_NOT_FOUND))
                          {
                              APP_ERROR_CHECK(err_code);
                          }
                      } while (err_code == NRF_ERROR_RESOURCES);
                  }

                  index = 0;
              }
            }
            break;

        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;

        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
    //app_util_critical_region_exit(nest);
}
/**@snippet [Handling the data received over UART] */



/**@brief  Function for initializing the UART module.
 */
/**@snippet [UART Initialization] */
static void uart_init(void)
{
    uint32_t                     err_code;
    app_uart_comm_params_t const comm_params =
    {
        .rx_pin_no    = RX_PIN_NUMBER,
        .tx_pin_no    = TX_PIN_NUMBER,
        .rts_pin_no   = RTS_PIN_NUMBER,
        .cts_pin_no   = CTS_PIN_NUMBER,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity   = false,
#if defined (UART_PRESENT)
        .baud_rate    = UART_BAUDRATE_BAUDRATE_Baud31250
#else
        .baud_rate    = UART_BAUDRATE_BAUDRATE_Baud31250
#endif
    };

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_LOWEST,
                       err_code);
    APP_ERROR_CHECK(err_code);
}
/**@snippet [UART Initialization] */


//Temporarily suspend external UART RX by changing its RX pin.
void suspendUARTRX(int fakePin) {
  noteFlag = 10;
  nrf_delay_ms(1);
  NRF_UART0 -> PSELRXD = fakePin;
}


//Re-enable UART RX after suspending it by changing the RX pin back.
void resumeUARTRX() {
  //NRF_UART0 -> TASKS_SUSPEND;
  NRF_UART0 -> PSELRXD = RX_PIN_NUMBER;
  nrf_delay_ms(1);
  noteFlag = 0;
}


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}


/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    bsp_event_t startup_event;

    uint32_t err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}

void midi_scheduled_event(void* p_event_data, uint16_t event_size){
    //printf("midi_scheduled_event started\r\n");
    unsigned long next_delay_ms = read_next_midi_data();
    midi_delay(next_delay_ms); 
}

static void midi_delay_done_handler(void* p_context){
    UNUSED_PARAMETER(p_context);
    //app_sched_event_put(&sd_evt, sizeof(sd_evt), midi_scheduled_event);
    unsigned long next_delay_ms = read_next_midi_data();
    //printf("Delaying %d ms\r\n", next_delay_ms);
    midi_delay(next_delay_ms); 
}

void midi_delay(unsigned long time_ms){
    if (time_ms < 0) {
        return;
    }
    if (time_ms == 0) {
      midi_delay(read_next_midi_data());
    }
    else {
      APP_TIMER_DEF(midi_timer);
      ret_code_t err_code;
      err_code = app_timer_create(&midi_timer, APP_TIMER_MODE_SINGLE_SHOT, midi_delay_done_handler);
      APP_ERROR_CHECK(err_code);

      err_code = app_timer_start(midi_timer, APP_TIMER_TICKS(time_ms), NULL);
      APP_ERROR_CHECK(err_code);
     }
  }

/**@brief Application main function.
 */
int main(void)
{
    bool erase_bonds;

    // Initialize BLE.
    uart_init();
    //log_init();
    timers_init();
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
    buttons_leds_init(&erase_bonds);
    power_management_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();
    // This command below can be used to check if sd card is connected properly; comment out if using scheduler!!!!!!!!
    // fatfs_init();
    //fatfs_example();
    

    // Initialize FATFS
    bsp_board_init(BSP_INIT_LEDS);

    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
    //NRF_LOG_DEFAULT_BACKENDS_INIT();
    NRF_LOG_INFO("FATFS example started.");


    // Start execution.
    printf("\r\nUART started.\r\n");
    NRF_LOG_INFO("Debug logging for UART over RTT started.");
    advertising_start();

    // LEDs
    initialize_led_strip(144, 25);

    midi_delay(init_midi_file("TEST.MID"));

    // Enter main loop.
    
    for (;;)
    {
        idle_state_handle();
        app_sched_execute();
    }
}
