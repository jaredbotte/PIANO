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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "nordic_common.h"
#include "nrf.h"
#include "ff.h"
#include "diskio_blkdev.h"
#include "nrf_block_dev_sdc.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_gpio.h"
#include "app_timer.h"
#include "ble_nus.h"
#include "app_uart.h"
#include "app_util_platform.h"
#include "nrf_pwr_mgmt.h"
#include "leds.h"
#include "nrf_delay.h"
#include "app_error.h"
#include "sd_card.h"
#include "app_scheduler.h"
#include "midifile.h"

// UART
#if defined (UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined (UARTE_PRESENT)
#include "nrf_uarte.h"
#endif

// BLE
#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */
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
#define SCHED_QUEUE_SIZE                20

//Globals
sd_write_evt sd_evt;
sd_write_buffer sd_data_buffer = {.length = 0, .data = {0x00}};
Color led_color     = {.red = 0, .green = 0, .blue = 0};
typedef enum states{VIS, LTP, PA}Mode;
bool tempoChanged   = false;
bool colorChanged   = false;
bool rChanged       = false;
bool gChanged       = false;
bool bChanged       = false;
bool writeEnable    = false;
bool bufferBusy     = false;
bool bufferLock     = false;
bool transfer_over  = false;
volatile bool prevSD         = false;
volatile bool fileTransfer   = false;
volatile bool bufferFilled   = false;
volatile bool transferStarted = false;
volatile bool playNameChange = false;
volatile bool isConnected = false;
volatile bool hasSDCard = true;
volatile bool stateChanged = false;
volatile bool velo = false;
Color userColor = GREEN;
volatile Mode currentMode;
char filename[12];
//char fileToPlay[12] = {0x50,0x48,0x49,0x4c,0x30,0x2e,0x4d,0x49,0x44,0x0d,0x00,0x00}; //this is phil0
char fileToPlay[12];
int bytes_w = 0, bytes_s = 0, num_received = 0, num_written = 0;
static int noteFlag = 0;
static uint8_t lastEvent = 0;
static uint8_t keyNum = 0x00;

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint16_t   m_ble_nus_max_data_len = 512 - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

//Begin Functions
void led_update (void* p_event_data, uint16_t event_size) {
  update_led_strip();
}


static void correct_delay_handler(void* p_context){
    //MidiEvent* mevt = (MidiEvent*) p_context;
    uint8_t* delayNote = (uint8_t*) p_context;
    if(delayNote != NULL) {
        printf("Delay key %i\r\n", delayNote);
        set_key(*delayNote, true, true, 1, LEARN_COLOR);
        free(delayNote);
    }
    printf("BAD TIMES\r\n");
}


void learnDelay (void* p_event_data, uint16_t event_size) {    
    sd_write_evt* evt = (sd_write_evt*) p_event_data;
    if(evt != NULL) {
        uint8_t* delayNote = malloc(sizeof(uint8_t)); 
        *delayNote = evt->note;
        APP_TIMER_DEF(correct_delay);
        ret_code_t err_code;
        err_code = app_timer_create(&correct_delay, APP_TIMER_MODE_SINGLE_SHOT, correct_delay_handler);
        APP_ERROR_CHECK(err_code);

        err_code = app_timer_start(correct_delay, APP_TIMER_TICKS(500), delayNote);
        APP_ERROR_CHECK(err_code);
        free(evt);
    }
    printf("SAD TIMES\r\n");
}


void send_Message (char* msg) {
    uint16_t size = strlen(msg);
    ble_nus_data_send(&m_nus, msg, &size, m_conn_handle);
}


void TIMER3_IRQHandler(void)
{
  NRF_TIMER3->EVENTS_COMPARE[0] = 0;	
  app_sched_event_put(&sd_evt, sizeof(sd_evt), led_update);

  /*//NOTE have not been tested
  hasSDCard = nrf_gpio_pin_read(30);
  if (isConnected) {
    if (hasSDCard != prevSD) {
      if (hasSDCard) {
        send_Message("hasSD");
      }
      else{
        send_Message("noSD");
      }
      prevSD = hasSDCard;
    }
  }*/
}

void fileWrite(void* p_event_data, uint16_t event_size) {
    static FATFS fs;
    static DIR dir;
    static FILINFO fno;
    static FIL file;

    uint32_t bytes_written;
    FRESULT ff_result;
    DSTATUS disk_state = STA_NOINIT;
    sd_write_evt* evt = (sd_write_evt*) p_event_data;

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

    char fn[32];
    strcpy(fn, evt->filename);

    ff_result = f_open(&file,fn, FA_READ | FA_WRITE | FA_OPEN_APPEND);
    if (ff_result != FR_OK)
    {
        printf("File open failed. Error type %d\r\n", ff_result);
        return;
    }

    ff_result = f_write(&file, evt->buf.data, evt->buf.length, (UINT *) &bytes_written);
    if (ff_result != FR_OK)
    {
       printf("Write failed.\r\n.");
    }
    else
    {
        num_written++;
        bytes_w+=bytes_written;
        printf("%d bytes has been written\r\n", bytes_w);

        // char* msg = "rcvd";
        // uint16_t size = strlen(msg);
        // ble_nus_data_send(&m_nus, msg, &size, m_conn_handle);
        send_Message("rcvd");
    }

    (void) f_close(&file);
    return;
}

void list_Directory (void* p_event_data, uint16_t event_size){
    static FATFS fs;
    static DIR dir;
    static FILINFO fno;
    static FIL file;

    uint32_t bytes_written;
    FRESULT ff_result;
    DSTATUS disk_state = STA_NOINIT;

    send_Message("Listing");
    ff_result = f_mount(&fs, "", 1);
    ff_result = f_opendir(&dir, "/");
    if (ff_result)
    {
        printf("Directory listing failed with code %d!\r\n", ff_result);
        return;
    }

    do
    {
        ff_result = f_readdir(&dir, &fno);
        if (ff_result != FR_OK)
        {
            printf("Directory read failed with code %d.\r\n", ff_result);
            return;
        }

        if (fno.fname[0])
        {
            if (fno.fattrib & AM_DIR)
            {
                printf("   <DIR>   %s\r\n",(uint32_t)fno.fname);
            }
            else
            {
                printf("%9lu  %s", fno.fsize, (uint32_t)fno.fname);
                send_Message((uint32_t)fno.fname);
            }
        }
    }
    while (fno.fname[0]);

    send_Message("list_end");

    return;
}

void file_check(void* p_event_data, uint16_t event_size){
     static FATFS fs;
    static DIR dir;
    static FILINFO fno;
    static FIL file;

    uint32_t bytes_written;
    FRESULT ff_result;
    DSTATUS disk_state = STA_NOINIT;

    ff_result = f_mount(&fs, "", 1);
    ff_result = f_opendir(&dir, "/");
    if (ff_result)
    {
        printf("Directory listing failed with code %d!\r\n", ff_result);
        return;
    }

    do
    {
        ff_result = f_readdir(&dir, &fno);
        if (ff_result != FR_OK)
        {
            printf("Directory read failed with code %d.\r\n", ff_result);
            return;
        }

        if (fno.fname[0])
        {
            if (fno.fattrib & AM_DIR)
            {
                printf("   <DIR>   %s\r\n",(uint32_t)fno.fname);
            }
            else
            {
                printf("%9lu  %s", fno.fsize, (uint32_t)fno.fname);
                if (strcmp(&fno.fname, filename) == 0) {
                    // NOTE: This may not work
                    f_unlink(&fno.fname);
                }
            }
        }
    }
    while (fno.fname[0]);

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

char device_name[18];

/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    // Let's determine the device "unique" name.

    sprintf(&device_name, "P.I.A.N.O. %x", NRF_FICR -> DEVICEID[0]);
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) device_name,
                                          strlen(device_name));
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

float stupid_atof (char* msg) {
  if (strlen(msg) < 3)
    return -1.0;

  char val[3];

  val[0] = msg[0];
  val[1] = msg[2];

  if (msg[3] != '\0') {
    val[2] = msg[3];
  }
  else {
    val[2] = '0';
  }

  int int_val = atoi(&val);
  float f_val = (float)(int_val/100.);

  return f_val;
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
        uint8_t data[BLE_BUF_SIZE];
        memset(&data, 0,BLE_BUF_SIZE);

        for (uint32_t i = 0; i < p_evt->params.rx_data.length; i++) {
            data[i] = p_evt->params.rx_data.p_data[i];
            sd_evt.buf.data[i] = p_evt->params.rx_data.p_data[i];
        }
        int size = p_evt->params.rx_data.length;
        sd_evt.buf.length = p_evt->params.rx_data.length;

        // Start of critical system commands!
        if (strncmp(&data, "VIS", 3) == 0) {
            if (currentMode != VIS){
                currentMode = VIS;
                stateChanged = true;
            }
         }

         if (strncmp(&data, "LTP", 3) == 0) {
            if (currentMode != LTP){
                currentMode = LTP;
                stateChanged = true;
            }   
         }

         if (strncmp(&data, "TEMPO", 5) == 0) {
            tempoChanged = true;
            return;
         }
        
         if (strncmp(&data, "CheckSD", 7) == 0) {
            hasSDCard = nrf_gpio_pin_read(30);
            if(hasSDCard){
                send_Message("hasSD");
            } else {
                send_Message("noSD");
            }
         }

         if (tempoChanged == true){
            data[strlen(data) - 1] = '\0';  
            setTempoDiv(stupid_atof(&data));
            tempoChanged = false;
         }

         if (strncmp(&data, "PA", 2) == 0) {
            if (currentMode != PA){
                currentMode = PA;
                stateChanged = true;
            }
         }

         if (strncmp(&data, "PFILE", 5) == 0) {
            playNameChange = true;
            return;
         }

         if (playNameChange) {
            // TODO: Make sure this name complies!!
            strcpy(&fileToPlay, &data);
            playNameChange = false;
         }
        // End of critical system commands!

        if (strncmp(&data, "write", 5) == 0){
            sd_evt.filename = "test.txt";
            app_sched_event_put(&sd_evt, sizeof(sd_evt), fileWrite);
        }

        if (strncmp(&data, "blue",4) == 0) {
            Color blue = {.red = 0, .green = 0, .blue = 60};
            fill_color(blue);
            
        }

        if (strncmp(&data, "red",3) == 0) {
            Color red = {.red = 60, .green = 0, .blue = 0};
            fill_color(red);
            err_code = ble_nus_data_send(&m_nus, "eof", 3, m_conn_handle);
        }

        if (strncmp(&data, "green",5) == 0) {
            Color green = {.red = 0, .green = 60, .blue = 0};
            fill_color(green);
        }

        if (strncmp(&data, "off",3) == 0) {
            Color off = {.red = 0, .green = 0, .blue = 0};
            fill_color(off);
        }

        if (strncmp(&data, "Color_Changed",13) == 0) {
            colorChanged = true;
            rChanged == false;
            gChanged == false;
            bChanged == false;
            return;
        }

        if(strncmp(&data, "anim", 4) == 0){
            led_connect_animation();
        }

        if(strncmp(&data, "velo_false", 10) == 0){
            velo = false;
        }

        if(strncmp(&data, "velo_true", 9) == 0){
            velo = true;
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
            userColor.red = color_value;

            colorChanged = false;
            rChanged = false;
        }
        else if ((colorChanged == true) && (rChanged == false) && (gChanged == true) && (bChanged == false) && (strncmp(&data, "green",5) != 0)) {
            int color_value = atoi(&data);
            userColor.green = color_value;

            colorChanged = false;
            gChanged = false;
        }
        else if ((colorChanged == true) && (rChanged == false) && (gChanged == false) && (bChanged == true) && (strncmp(&data, "blue",4) != 0)) {
            int color_value = atoi(&data);
            userColor.blue = color_value;

            colorChanged = false;
            bChanged = false;
        }

        if (strncmp(&data, "ListDIR",7) == 0) {
            app_sched_event_put(&sd_evt, sizeof(sd_evt), list_Directory);
        }


        // File transfer
        if (strncmp(&data, "EOF",3) == 0) {
            fileTransfer = false;
            transferStarted = false;
            send_Message("eof");
            printf("\r\nTransfer Completed\r\n");
            transfer_over = true;
            printf("\r\nNumber of reception: %d\r\n", num_received);
            printf("Number of write: %d\r\n", num_written);
            printf("Filename: %s\r\n", filename);
            memset(filename,'\0', sizeof(filename));
            //resumeUARTRX();
            return;
        }
        
        if (strncmp(&data, "File Transfer",13) == 0) {
            send_Message("Transfer");
            fileTransfer = true;
            //suspendUARTRX();
            return;
        }

        if (fileTransfer && !transferStarted) {
            strncpy(&filename, &data, 11);
            transferStarted = true;
            filename[strlen(filename)-1] = '\0';
            app_sched_event_put(&sd_evt, sizeof(sd_evt), file_check);     
            return;
        }

        if (fileTransfer && transferStarted) {
                bufferBusy = true;
                for (uint32_t i = 0; i < size; i++) {
                    sd_data_buffer.data[i] = data[i];
                }
                sd_data_buffer.length = size;
                num_received++;
                bufferFilled = true;
                bufferBusy   = false;
                sd_evt.filename = filename;
                app_sched_event_put(&sd_evt, sizeof(sd_evt), fileWrite); 
                return;      
        }
    
        for (uint32_t i = 0; i < p_evt->params.rx_data.length; i++)
        {
          do
          {
            err_code = app_uart_put(p_evt->params.rx_data.p_data[i]);
            if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_BUSY))
            {
              APP_ERROR_CHECK(err_code);
            }
          } while (err_code == NRF_ERROR_BUSY);
        }
        if (p_evt->params.rx_data.p_data[p_evt->params.rx_data.length - 1] == '\r')
        {
              while (app_uart_put('\n') == NRF_ERROR_BUSY);
        }
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
    uint32_t err_code = NRF_SUCCESS;
    ledIndicate(LED_IDLE);
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
            err_code = NRF_SUCCESS;
            ledIndicate(LED_ADVERTISING);
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
            err_code = NRF_SUCCESS;
            ledIndicate(LED_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            isConnected = true;
            err_code = ble_nus_data_send(&m_nus, "System Initialized", 19, m_conn_handle);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            nrf_gpio_pin_clear(6);
            isConnected = false;
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
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
    }
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


/**@brief   Function for handling app_uart events.
 *
 * @details This function will receive a single character from the app_uart module and append it to
 *          a string. The string will be be sent over BLE when the last character received was a
 *          'new line' '\n' (hex 0x0A) or if the string has reached the maximum data length.
 */
/**@snippet [Handling the data received over UART] */
void uart_event_handle(app_uart_evt_t * p_event)
{
    static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
    static uint8_t index = 0;
    uint32_t       err_code;
    uint8_t eventUART = 0x00;

    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:  
            while(app_uart_get(&eventUART) != NRF_SUCCESS); //Pull a byte off the FIFO
            if(noteFlag == 0 && (eventUART == 0x90 || eventUART == 0x80)) { //Event headder
              noteFlag = 1;
              lastEvent = eventUART;
            }
            else if(noteFlag == 1) //Note number
            {
              int keyOffset = 21;
              keyNum = eventUART - keyOffset;
              noteFlag = 2;
            }
            else if(noteFlag == 2) //Velocity info
            {
              bool type = lastEvent == 0x90 ? true : false; 
              if (currentMode == VIS) {
                if (velo) set_key_velocity(keyNum, type, eventUART);
                else set_key(keyNum, type, false, eventUART, userColor);
              } 
              else if (currentMode == PA) {
                set_key_play(keyNum, type, eventUART);
              } 
              else if (currentMode == LTP) {
                set_key_learn(keyNum, type, eventUART);
                if(isLearnSetFinished()) {
                    learn_next_midi_data();
                    //printf("keys to press: %d\r\n", numKeysToPress);
                }
              }
              noteFlag = 0;
            }
            else
            {
              data_array[index] = eventUART; //Give the data back to BLE if it was not a MIDI event.
              index++;

              if ((data_array[index - 1] == '\n') ||
                  (data_array[index - 1] == '\r') ||
                  (index >= m_ble_nus_max_data_len))
              {
                  if (index > 1)
                  {
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
        .rx_pin_no    = 8,
        .tx_pin_no    = 11,
        .rts_pin_no   = 8,
        .cts_pin_no   = 9,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity   = false,
        .baud_rate    = UART_BAUDRATE_BAUDRATE_Baud31250
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
    nrf_pwr_mgmt_run();
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}

void midi_scheduled_event(void* p_event_data, uint16_t event_size){
    unsigned long next_delay_ms = read_next_midi_data();
    midi_delay(next_delay_ms); 
}

static void midi_delay_done_handler(void* p_context){
    unsigned long next_delay_ms = read_next_midi_data();
    midi_delay(next_delay_ms); 
}


void midi_delay(unsigned long time_ms){
    if (currentMode == VIS) {
        fill_color(OFF);
        return;
    }
    if (time_ms < 0) {
        currentMode = VIS;
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

void midi_operations() {
    if (stateChanged){
        resetKeys();
        // TODO: Make sure the phone knows this bool! Otherwise states will mis-match
        if (currentMode == LTP && hasSDCard){
            printf("Now in LTP\r\n");
            UNUSED_PARAMETER(init_midi_file(fileToPlay));
            learn_next_midi_data();
        } 
        else if (currentMode == PA && hasSDCard){
            printf("Now in PA\r\n");
            midi_delay(init_midi_file(fileToPlay));
        } 
        else {
            printf("SD? %s\r\n", hasSDCard ? "true" : "false");
            printf("Now in VIS\r\n");
            currentMode = VIS;
            fill_color(OFF);
        }
        stateChanged = false;
    }

    return;
}

/**@brief Application main function.
 */
int main(void) 
{
    uart_init();
    timers_init();
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
    power_management_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();
    fatfs_init();

    initIndication();
    initialize_led_strip(288, 25, 88);
    nrf_gpio_cfg_input(30, NRF_GPIO_PIN_NOPULL);

    // Start execution.
    printf("\r\nUART started.\r\n");
    advertising_start();

    for (;;)
    {
        idle_state_handle();
        app_sched_execute();
        midi_operations();
    }
}