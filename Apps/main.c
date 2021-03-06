/**
* Copyright (c) 2014 - 2017, Nordic Semiconductor ASA
* 
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
* 
* 1. Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following disclaimer.
* 
* 2. Redistributions in binary form, except as embedded into a Nordic
*    Semiconductor ASA integrated circuit in a product or a software update for
*    such product, must reproduce the above copyright notice, this list of
*    conditions and the following disclaimer in the documentation and/or other
*    materials provided with the distribution.
* 
* 3. Neither the name of Nordic Semiconductor ASA nor the names of its
*    contributors may be used to endorse or promote products derived from this
*    software without specific prior written permission.
* 
* 4. This software, with or without modification, must only be used with a
*    Nordic Semiconductor ASA integrated circuit.
* 
* 5. Any software provided in binary form under this license must not be reverse
*    engineered, decompiled, modified and/or disassembled.
* 
* THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
*/
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

#include <stdint.h>
#include <string.h>
#include "board.h"
#include "nordic_common.h"
#include "nrf.h"
#include "gps.h"
#include "utils.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_fstorage.h"
#include "nrf_ble_gatt.h"
#include "app_timer.h"
#include "ble_nus.h"
#include "app_uart.h"
#include "app_util_platform.h"
#include "bsp_btn_ble.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

//#include "nrf_ble_qwr.h"
//#include "nrf_ble_qwrs.h"

#include "nrf_drv_twi.h"
#include "rak_i2c_drv.h"
#include "rak_i2c_sht31.h"
#include "rak_i2c_oled.h"
#include "custom_board.h"
#include "rak_i2c_gps_max7.h"
#include "lis3dh_driver.h"


#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */

#define APP_FEATURE_NOT_SUPPORTED       BLE_GATT_STATUS_ATTERR_APP_BEGIN + 2        /**< Reply when unsupported features are requested. */

#define DEVICE_NAME                     "RAK813 BreakBoard"                               /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_BLE //BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_BLE_OBSERVER_PRIO           1                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#define APP_ADV_INTERVAL                64                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS      180                                         /**< The advertising timeout (in units of seconds). */

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


BLE_NUS_DEF(m_nus);                                                                 /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */


static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};


//#define MEM_BUFF_SIZE                    512
////NRF_BLE_QWR_DEF(m_qwr);       																								/**< Queued Writes structure.*/
//static uint8_t      m_buffer[MEM_BUFF_SIZE];
////static nrf_ble_qwrs_t m_qwrs;
//

lora_cfg_t		g_lora_cfg;																													/**< lorab board config param.*/
bool loraconfigupdataflg = false;

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


/**@brief Function for handling the data from the Nordic UART Service.
*
* @details This function will process the data received from the Nordic UART BLE Service and send
*          it to the UART module.
*
* @param[in] p_nus    Nordic UART Service structure.
* @param[in] p_data   Data to be send to UART module.
* @param[in] length   Length of the data.
*/
/**@snippet [Handling the data received over BLE] */


int set_handler(int argc, char* argv[], lora_cfg_t* cfg_info)
{
    
    NRF_LOG_INFO("%s=%s", argv[0], argv[1]);
    
    if (strcmp(argv[0], "dev_eui") == 0) {
        StrToHex((char *)cfg_info->dev_eui, argv[1], 8);
    }
    else if (strcmp(argv[0], "app_eui") == 0) {
        StrToHex((char *)cfg_info->app_eui, argv[1], 8);
    }
    else if (strcmp(argv[0], "app_key") == 0) {
        StrToHex((char *)cfg_info->app_key, argv[1], 16);
    }
    else if (strcmp(argv[0], "dev_addr") == 0) {
        cfg_info->dev_addr = strtoul(argv[1], NULL, 16);
    }
    else if (strcmp(argv[0], "nwkskey") == 0) {
        StrToHex((char *)cfg_info->nwkskey, argv[1], 16);
    }
    else if (strcmp(argv[0], "appskey") == 0) {
        StrToHex((char *)cfg_info->appskey, argv[1], 16);
    }
    else
    {
        return -1;  
    }
    return 0;
}

int  parse_lora_config(char* str, lora_cfg_t *cfg)
{
    int argc;
    char* buf, *temp;
    char* argv[10];
    uint8_t flag = 1;
    
    do{
        argc = 0;
        if ((temp = strchr(str, '&')) != NULL) {
            *temp = '\0';
        }
        else
            flag = 0;
        
        if ((buf = strchr(str, '=')) != NULL) {
            argv[0] = str;
            if (*(buf + 1) == '\0')
                argc = 1;
            else {
                argc = 2;
                argv[1] = buf + 1;
            }
            *buf = '\0';
        }
        else {
            NRF_LOG_INFO("err\r\n");
            return -1;
        }
        if (argc > 0) {  
            if (set_handler(argc, argv, cfg) != 0) {
                return -1;
            }
        }
        else {
            return -1;
        }
        str = ++temp;
        
    } while(flag); 
    
    return 0;
}

static void nus_data_handler(ble_nus_evt_t * p_evt)
{
    
    if (p_evt->type == BLE_NUS_EVT_RX_DATA)
    {
        uint32_t err_code;
        
        NRF_LOG_DEBUG("Received data from BLE NUS. Writing data on UART.");
        NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
        
        
        NRF_LOG_DEBUG("recv len=%d", p_evt->params.rx_data.length);
        if(strncmp((char*)p_evt->params.rx_data.p_data, "lora_cfg:", strlen("lora_cfg:")) == 0)
        {
            parse_lora_config((char*)p_evt->params.rx_data.p_data+strlen("lora_cfg:"), &g_lora_cfg);
            loraconfigupdataflg = true;				
        }
        else
        {
#if 1
            for (uint32_t i = 0; i < p_evt->params.rx_data.length; i++)
            {
                do
                {
                    err_code = app_uart_put(p_evt->params.rx_data.p_data[i]);
                    if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_BUSY))
                    {
                        NRF_LOG_ERROR("Failed receiving NUS message. Error 0x%x. ", err_code);
                        APP_ERROR_CHECK(err_code);
                    }
                } while (err_code == NRF_ERROR_BUSY);
            }
            if (p_evt->params.rx_data.p_data[p_evt->params.rx_data.length-1] == '\r')
            {
                while (app_uart_put('\n') == NRF_ERROR_BUSY);
            }
#endif
        }
        
    }
    
}
/**@snippet [Handling the data received over BLE] */


/**@brief Function for handling Service errors.
*
* @details A pointer to this function will be passed to each service which may need to inform the
*          application about an error.
*
* @param[in]   nrf_error   Error code containing information about what went wrong.
*/
static void service_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for initializing services that will be used by the application.
*/
static void services_init(void)
{
    uint32_t       err_code;
    ble_nus_init_t nus_init;
    
    memset(&nus_init, 0, sizeof(nus_init));
    
    nus_init.data_handler = nus_data_handler;
    
    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
    
#if 0	
    nrf_ble_qwr_init_t  qwr_init;
    nrf_ble_qwrs_init_t qwrs_init;
    
    // Initialize Queued Write Module
    qwr_init.mem_buffer.len   = MEM_BUFF_SIZE;
    qwr_init.mem_buffer.p_mem = m_buffer;
    qwr_init.error_handler    = service_error_handler;
    qwr_init.callback         = queued_write_handler;
    
    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);
    
    //initialize the Queued Writes Example Service
    memset(&qwrs_init, 0, sizeof(qwrs_init));
    
    qwrs_init.evt_handler   = queued_write_example_service_evt_handler;
    qwrs_init.error_handler = service_error_handler;
    qwrs_init.p_qwr_ctx     = &m_qwr;
    
    err_code = nrf_ble_qwrs_init(&qwrs_init, &m_qwrs);
    APP_ERROR_CHECK(err_code);
#endif
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
        break;
        
      case BLE_GAP_EVT_DISCONNECTED:
        NRF_LOG_INFO("Disconnected");
        err_code = bsp_indication_set(BSP_INDICATE_IDLE);
        APP_ERROR_CHECK(err_code);
        m_conn_handle = BLE_CONN_HANDLE_INVALID;
        break;
        
#if defined(S132)
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
#endif
        
      case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
        // Pairing not supported
        err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
        APP_ERROR_CHECK(err_code);
        break;
        
      case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
        {
            ble_gap_data_length_params_t dl_params;
            
            // Clearing the struct will effectivly set members to @ref BLE_GAP_DATA_LENGTH_AUTO
            memset(&dl_params, 0, sizeof(ble_gap_data_length_params_t));
            err_code = sd_ble_gap_data_length_update(p_ble_evt->evt.gap_evt.conn_handle, &dl_params, NULL);
            APP_ERROR_CHECK(err_code);
        } break;
        
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
        
      case BLE_EVT_USER_MEM_REQUEST:
        err_code = sd_ble_user_mem_reply(p_ble_evt->evt.gattc_evt.conn_handle, NULL);
        APP_ERROR_CHECK(err_code);
        break;
        
      case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
        {
            ble_gatts_evt_rw_authorize_request_t  req;
            ble_gatts_rw_authorize_reply_params_t auth_reply;
            
            req = p_ble_evt->evt.gatts_evt.params.authorize_request;
            
            if (req.type != BLE_GATTS_AUTHORIZE_TYPE_INVALID)
            {
                if ((req.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ)     ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) ||
                        (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL))
                {
                    if (req.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                    }
                    else
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
                    }
                    auth_reply.params.write.gatt_status = APP_FEATURE_NOT_SUPPORTED;
                    err_code = sd_ble_gatts_rw_authorize_reply(p_ble_evt->evt.gatts_evt.conn_handle,
                                                               &auth_reply);
                    APP_ERROR_CHECK(err_code);
                }
            }
        } break; // BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST
        
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
    
    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, 240);
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
void uart_event_handle(app_uart_evt_t * p_event)
{
    static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
    static uint8_t index = 0;
    uint32_t       err_code;
    
    switch (p_event->evt_type)
    {
      case APP_UART_DATA_READY:
        UNUSED_VARIABLE(app_uart_get(&data_array[index]));
        index++;
        
        if ((data_array[index - 1] == '\n') || (index >= (m_ble_nus_max_data_len)))
        {
            NRF_LOG_DEBUG("Ready to send data over BLE NUS");
            NRF_LOG_HEXDUMP_DEBUG(data_array, index);
            
            do
            {
                uint16_t length = (uint16_t)index;
                err_code = ble_nus_string_send(&m_nus, data_array, &length);
                if ( (err_code != NRF_ERROR_INVALID_STATE) && (err_code != NRF_ERROR_BUSY) )
                {
                    APP_ERROR_CHECK(err_code);
                }
            } while (err_code == NRF_ERROR_BUSY);
            
            index = 0;
        }
        break;
        
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
        .rx_pin_no    = UART_RXD_PIN,
        .tx_pin_no    = UART_TXD_PIN,
        .rts_pin_no   = UART_RTS_PIN,
        .cts_pin_no   = UART_CTS_PIN,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity   = false,
        .baud_rate    = UART_BAUDRATE_BAUDRATE_Baud115200
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
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;
    
    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;
    
    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_TIMEOUT_IN_SECONDS;
    
    init.evt_handler = on_adv_evt;
    
    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);
    
    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}


/**@brief Function for initializing buttons and leds.
*
* @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
*/
//static void buttons_leds_init(bool * p_erase_bonds)
//{
//    bsp_event_t startup_event;
//    
//    uint32_t err_code = bsp_init(BSP_INIT_LED | BSP_INIT_BUTTONS, bsp_event_handler);
//    APP_ERROR_CHECK(err_code);
//    
//    err_code = bsp_btn_ble_init(NULL, &startup_event);
//    APP_ERROR_CHECK(err_code);
//    
//    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
//}


/**@brief Function for initializing the nrf log module.
*/
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    
    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for placing the application in low power state while waiting for events.
*/
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}

/*
i2c device init
*/
uint32_t i2c_drv_init(void)
{
    uint32_t err_code;
    
    const nrf_drv_twi_config_t twi_lis_config = {
        .scl                = DEV_TWI_SCL_PIN,
        .sda                = DEV_TWI_SDA_PIN,
        .frequency          = NRF_TWI_FREQ_400K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGHEST
    };
    
    err_code = rak_i2c_init(&twi_lis_config);
    if(err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    return NRF_SUCCESS;
}



/**@brief Application main function.
*/
#include "nrf_delay.h"
#include "timer.h"
#include "board.h"

extern char StringText[100];
//static TimerTime_t GpsCommsTimer=0;

APP_TIMER_DEF(read_gps_timer);
APP_TIMER_DEF(read_HT_timer);
APP_TIMER_DEF(read_3DH_timer);

extern uint8_t GpsDataBuffer[512];
bool readgpsdatastreamflag = false;
bool readHTdatastreamflag = false;
bool read3DHdatastreamflag = false;
int SECOND_TICK = 32768;
void* read_gps_timer_handle()
{
    readgpsdatastreamflag = true;
    return NULL;
}

uint32_t read_gps_timer_init(void)
{
    app_timer_create(&read_gps_timer,APP_TIMER_MODE_REPEATED,(app_timer_timeout_handler_t)read_gps_timer_handle);
    app_timer_start(read_gps_timer, SECOND_TICK*0.1, NULL);
    return NULL;
}

void* read_HT_timer_handle()
{
    readHTdatastreamflag = true;
    return NULL;
}

uint32_t read_HT_timer_init(void)
{
    app_timer_create(&read_HT_timer,APP_TIMER_MODE_REPEATED,(app_timer_timeout_handler_t)read_HT_timer_handle);
    app_timer_start(read_HT_timer, SECOND_TICK*5, NULL);
    return NULL;
}

void* read_3DH_timer_handle()
{
    read3DHdatastreamflag = true;
    return NULL;
}

uint32_t read_3DH_timer_init(void)
{
    app_timer_create(&read_3DH_timer,APP_TIMER_MODE_REPEATED,(app_timer_timeout_handler_t)read_3DH_timer_handle);
    app_timer_start(read_3DH_timer, SECOND_TICK*5, NULL);
    return NULL;
}

peripherals_data per_data;

void Write_OLED_string(unsigned char* status)
{
    char len = strlen((char const*)status);
    len = (21- len)/2;
    char line1[25];
    char line2[25];
    char line3[25];
    char line4[25];
    char line5[25];
    
    sprintf(line1,"Battery:%d%\0",per_data.battery);
    sprintf(line2,"T:%.1fC, H:%.1f%\0",per_data.HT_temperature, per_data.HT_humidity);
    sprintf(line3,"%.2fN,%.2fE,%dM\0",per_data.gps_latitude,per_data.gps_longitude,per_data.gps_altitude);
    sprintf(line4,"X:%d Y:%d\0",per_data.MEMS_X,per_data.MEMS_Y);
    sprintf(line5,"Z:%d\0",per_data.MEMS_Z);
    
    OLED_Init();
    OLED_CLS();
    
    OLED_ShowStr(10,0,"RAK813 BreakBoard",1);
    OLED_ShowStr(0,1,(unsigned char*)line1,1);
    OLED_ShowStr(0,2,(unsigned char*)line2,1);
    OLED_ShowStr(0,3,(unsigned char*)line3,1);
    OLED_ShowStr(0,4,(unsigned char*)line4,1);
    OLED_ShowStr(0,5,(unsigned char*)line5,1);
    OLED_ShowStr(len*5,6,status,1);
}

void nRF_hardware_init()
{
    uint32_t err_code;
    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
    
    uart_init();
    log_init();
    /*i2c init*/
    i2c_drv_init();
    printf("nRE Hardware init success\r\n");
}

void nRF_BLE_init()
{
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();
    printf("nRF BLE init success.\r\n");
}

void nRF_lora_init()
{
    /* load lora configuration*/
    u_fs_init();
    memset((uint8_t*)&g_lora_cfg,0,sizeof(g_lora_cfg));
    u_fs_read_lora_cfg(&g_lora_cfg);
    u_fs_check_lora_cfg(&g_lora_cfg);
    lora_init(&g_lora_cfg);
    printf("LoRa init success.\r\n");
}
void nRF_timer_init()
{
    gps_setup(); 
    lis3dh_init();
    lis3dh_setup();
    read_gps_timer_init();
    read_HT_timer_init();
    read_3DH_timer_init();
}

int main(void)
{
    uint32_t err_code;
    //bool   erase_bonds;
    
    nRF_hardware_init();
    nRF_BLE_init();
    nRF_lora_init();
    nRF_timer_init();
    
    Write_OLED_string("DEVICE INIT");
    
    err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
    printf("nRF BLE advertising start.\r\n");
    
    // Enter main loop.
    while(1)
    {
        lora_process();
        
        if (!NRF_LOG_PROCESS())
        {
            power_manage();
        }
        
        if(loraconfigupdataflg)
        {
            u_fs_write_lora_cfg(&g_lora_cfg);
            loraconfigupdataflg = false;
        }
        if (readgpsdatastreamflag)
        {
            NRF_LOG_DEBUG("gps wait\r\n");
            Max7GpsReadDataStream();
            readgpsdatastreamflag = false;
            if (GpsParseGpsData(GpsDataBuffer, 512))
            {
                double lat,lon;
                uint8_t ret = GpsGetLatestGpsPositionDouble(&lat, &lon);
                per_data.gps_altitude = GpsGetLatestGpsAltitude();
                if(ret == SUCCESS )
                {
                    printf("lat=%f lot=%f \n", lat, lon);
                    per_data.gps_latitude = lat;
                    per_data.gps_longitude = lon;
                    Write_OLED_string("GPS UPDATE");
                }
            }
        }
        if (readHTdatastreamflag)
        {
            NRF_LOG_DEBUG("HT wait\r\n");
            readHTdatastreamflag = false;
            if (Sht31_startMeasurementHighResolution() == 0)
            {
                //Sht31_startMeasurementLowResolution();
                Sht31_readMeasurement_ft(&(per_data.HT_humidity),&(per_data.HT_temperature));
                printf("temperature:%.1fC , humidity��%.1f%\r\n\0",per_data.HT_temperature,per_data.HT_humidity);
                Write_OLED_string("HT UPDATE");
            }
            
        }
        if (read3DHdatastreamflag)
        {
            NRF_LOG_DEBUG("3DH wait\r\n");
            read3DHdatastreamflag = false;
            uint8_t cnt=1;
            AxesRaw_t data;
            uint8_t response;
            while(cnt--){
                //get Acceleration Raw data  
                response = LIS3DH_GetAccAxesRaw(&data);
                if(response==1){
                    printf("X=%d Y=%d Z=%d\r\n", data.AXIS_X, data.AXIS_Y, data.AXIS_Z);
                    per_data.MEMS_X = data.AXIS_X;
                    per_data.MEMS_Y = data.AXIS_Y;
                    per_data.MEMS_Z = data.AXIS_Z;
                }
            }
            Write_OLED_string("3DH UPDATE");
        }
        
        
    }
}

/**
* @}
*/
