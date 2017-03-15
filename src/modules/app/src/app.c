/**
 ****************************************************************************************
 *
 * @file app.c
 *
 * @brief Application entry point
 *
 * Copyright (C) 2012. Dialog Semiconductor Ltd, unpublished work. This computer
 * program includes Confidential, Proprietary Information and is a Trade Secret of
 * Dialog Semiconductor Ltd.  All use, disclosure, and/or reproduction is prohibited
 * unless authorized in writing. All Rights Reserved.
 *
 * <bluetooth.support@diasemi.com> and contributors.
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup APP
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "rwip_config.h"             // SW configuration

#if (BLE_APP_PRESENT)
#include "app_task.h"                // Application task Definition
#include "app.h"                     // Application Definition
#include "gapm_task.h"               // GAP Manager Task API
#include "gapc_task.h"               // GAP Controller Task API
#include "co_math.h"                 // Common Maths Definition
#include "app_api.h"                // Application task Definition
#include "app_sps_scheduler.h"

#if (BLE_APP_SEC)
#include "app_sec.h"                 // Application security Definition
#endif // (BLE_APP_SEC)

#if (NVDS_SUPPORT)
#include "nvds.h"                    // NVDS Definitions
#endif //(NVDS_SUPPORT)

/*
 * ENUMERATIONS
 ****************************************************************************************
 */
#include "app_user.h"

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/// Application Environment Structure
struct app_env_tag app_env __attribute__((section("retention_mem_area0"),zero_init)); //@RETENTION MEMORY

uint8_t app_sleep_flow_off __attribute__((section("retention_mem_area0"),zero_init)); //@RETENTION MEMORY

/*
 * LOCAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/// Application Task Descriptor
static const struct ke_task_desc TASK_DESC_APP = {NULL, &app_default_handler,
                                                  app_state, APP_STATE_MAX, APP_IDX_MAX};

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Application task initialization.
 *
 * @return void
 ****************************************************************************************
 */
void app_init(void)
{
    #if (NVDS_SUPPORT)
    uint8_t length = NVDS_LEN_SECURITY_ENABLE;
    #endif // NVDS_SUPPORT

    // Reset the environment
    memset(&app_env, 0, sizeof(app_env));

    // Initialize next_prf_init value for first service to add in the database
    app_env.next_prf_init = APP_PRF_LIST_START + 1;

    #if (NVDS_SUPPORT)
    // Get the security enable from the storage
    if (nvds_get(NVDS_TAG_SECURITY_ENABLE, &length, (uint8_t *)&app_env.sec_en) != NVDS_OK)
    #endif // NVDS_SUPPORT
    {
        // Set true by default (several profiles requires security)
        app_env.sec_en = true;
    }

    app_init_func();

    // Create APP task
    ke_task_create(TASK_APP, &TASK_DESC_APP);

    // Initialize Task state
    ke_state_set(TASK_APP, APP_DISABLED);

    #if (BLE_APP_SEC)
    app_sec_init();
    #endif // (BLE_APP_SEC)
}

/**
 ****************************************************************************************
 * @brief Profiles's Database initialization sequence.
 *
 * @return void
 ****************************************************************************************
 */
bool app_db_init(void)
{
    // Indicate if more services need to be added in the database
    bool end_db_create = false;

    end_db_create = app_db_init_func();

    return end_db_create;
}

/**
 ****************************************************************************************
 * @brief Send BLE disconnect command
 *
 * @return void
 ****************************************************************************************
 */
void app_disconnect(void)
{
    struct gapc_disconnect_cmd *cmd = KE_MSG_ALLOC(GAPC_DISCONNECT_CMD,
                                              KE_BUILD_ID(TASK_GAPC, app_env.conidx), TASK_APP,
                                              gapc_disconnect_cmd);

    cmd->operation = GAPC_DISCONNECT;
    cmd->reason = CO_ERROR_REMOTE_USER_TERM_CON;

    // Send the message
    ke_msg_send(cmd);
}

/**
 ****************************************************************************************
 * @brief Sends a connection confirmation message
 *
 * @param[in] auth      Authentication (@see gap_auth).
 *
 * @return void
 ****************************************************************************************
 */
void app_connect_confirm(uint8_t auth)
{
    // confirm connection
    struct gapc_connection_cfm *cfm = KE_MSG_ALLOC(GAPC_CONNECTION_CFM,
            KE_BUILD_ID(TASK_GAPC, app_env.conidx), TASK_APP,
            gapc_connection_cfm);

    cfm->auth = auth;
    cfm->authorize = GAP_AUTHZ_NOT_SET;

    // Send the message
    ke_msg_send(cfm);
}


/**
 ****************************************************************************************
 * Advertising Functions
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Start Advertising. Setup Advertsise and Scan Response Message
 *
 * @return void
 ****************************************************************************************
 */
void app_adv_start(void)
{

    // Allocate a message for GAP
    struct gapm_start_advertise_cmd *cmd = KE_MSG_ALLOC(GAPM_START_ADVERTISE_CMD,
                                                TASK_GAPM, TASK_APP,
                                                gapm_start_advertise_cmd);


    app_adv_func(cmd);

    // Send the message
    ke_msg_send(cmd);

    // We are now connectable
    ke_state_set(TASK_APP, APP_CONNECTABLE);
}

/**
 ****************************************************************************************
 * @brief Stop Advertising
 *
 * @return void
 ****************************************************************************************
 */
void app_adv_stop(void)
{
    // Disable Advertising
    struct gapm_cancel_cmd *cmd = KE_MSG_ALLOC(GAPM_CANCEL_CMD,
                                           TASK_GAPM, TASK_APP,
                                           gapm_cancel_cmd);

    cmd->operation = GAPM_CANCEL;

    // Send the message
    ke_msg_send(cmd);
}

/**
 ****************************************************************************************
 * @brief Send a connection param update request message
 *
 * @return void
 ****************************************************************************************
 */
void app_param_update_start(void)
{

    app_param_update_func();

}


/**
 ****************************************************************************************
 * @brief Start a kernel timer
 *
 * @param[in] timer_id      Timer identifier (message identifier type).
 * @param[in] task_id       Task identifier which will be notified
 * @param[in] delay         Delay in time units.
 *
 * @return void
 ****************************************************************************************
 */
void app_timer_set(ke_msg_id_t const timer_id, ke_task_id_t const task_id, uint16_t delay)
{
    // Delay shall not be more than maximum allowed
    if(delay > KE_TIMER_DELAY_MAX)
    {
        delay = KE_TIMER_DELAY_MAX;

    }
    // Delay should not be zero
    else if(delay == 0)
    {
        delay = 1;
    }

    ke_timer_set(timer_id, task_id, delay);
}


/**
 ****************************************************************************************
 * @brief Start scanning for an advertising device.
 *
 * @return void
 ****************************************************************************************
 */
void app_scanning(void)
{
    ke_state_set(TASK_APP, APP_CONNECTABLE);
    // create a kernel message to start the scanning
    struct gapm_start_scan_cmd *msg = (struct gapm_start_scan_cmd *)KE_MSG_ALLOC(GAPM_START_SCAN_CMD, TASK_GAPM, TASK_APP, gapm_start_scan_cmd);

	if(!key_bt_addr[0][0])//no key addr
    {
        da14580_sendBTaddressReq();
    }

    if(da14580_isHaveKey() && da14580_getTime() == TIMES_4_LOSE_KEY)
    {
        da14580_sendLockedReq();
    }
    da14580_addTime();

    msg->mode = GAP_GEN_DISCOVERY;
    msg->op.code = GAPM_SCAN_PASSIVE;
    msg->op.addr_src = GAPM_PUBLIC_ADDR;
    msg->filter_duplic = SCAN_FILT_DUPLIC_EN;
    msg->interval = APP_SCAN_INTERVAL;
    msg->window = APP_SCAN_WINDOW;
    // Send the message
    ke_msg_send(msg);
}

/**
 ****************************************************************************************
 * @brief Start the connection to the device with address connect_bdaddr
 *
 * @return void
 ****************************************************************************************
 */
void app_connect(void)
{
    struct gapm_start_connection_cmd *msg;
    // create a kenrel message to start connecting  with an advertiser
    msg = (struct gapm_start_connection_cmd *) KE_MSG_ALLOC(GAPM_START_CONNECTION_CMD , TASK_GAPM, TASK_APP, gapm_start_connection_cmd);
    // Maximal peer connection
    msg->nb_peers = 1;
    // Connect to a certain adress
    memcpy(&msg->peers[0].addr, &connect_bdaddr, BD_ADDR_LEN);

    msg->con_intv_min = APP_CON_INTV_MIN;
    msg->con_intv_max = APP_CON_INTV_MAX;
    msg->ce_len_min = APP_CE_LEN_MIN;
    msg->ce_len_max = APP_CE_LEN_MAX;
    msg->con_latency = APP_CON_LATENCY;
    msg->op.addr_src = GAPM_PUBLIC_ADDR;
    msg->peers[0].addr_type = GAPM_PUBLIC_ADDR;
    msg->superv_to = APP_CON_SUPERV_TO;
    msg->scan_interval = APP_CON_SCAN_INTERVAL;
    msg->scan_window = APP_CON_SCAN_WINDOW;
    msg->op.code = GAPM_CONNECTION_DIRECT;

    //set timer
    app_timer_set(APP_CONN_TIMER, TASK_APP, APP_CON_TIMEOUT);

    // Send the message
    ke_msg_send(msg);
}

/**
 ****************************************************************************************
 * @brief Stop scanning of the scanner
 *
 * @return void
 ****************************************************************************************
 */
void app_cancel_scanning(void)
{
    struct gapm_cancel_cmd *cmd =(struct gapm_cancel_cmd *) KE_MSG_ALLOC(GAPM_CANCEL_CMD, TASK_GAPM, TASK_APP, gapm_cancel_cmd);
    cmd->operation = GAPM_SCAN_PASSIVE; // Set GAPM_SCAN_PASSIVE
    ke_msg_send(cmd);// Send the message
}

/**
 ****************************************************************************************
 * @brief Sets BLE Rx irq threshold. It should be called at app_init and in external wakeup irq callabck if deep sleep is used.
 * @param[in] Rx buffer irq threshold
 * @return void
 ****************************************************************************************
 */
void app_set_rxirq_threshold(uint32_t rx_threshold)
{
    jump_table_struct[lld_rx_irq_thres] = (uint32_t) rx_threshold;
}

#endif //(BLE_APP_PRESENT)

/// @} APP
