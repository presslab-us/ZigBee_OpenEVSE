/**************************************************************************************************
  Filename:       zcl_openevse.c

  Description:    Zigbee Cluster Library - device application for OpenEVSE.


  Copyright 2006-2014 Texas Instruments Incorporated. All rights reserved.
  Copyright 2015 Ryan Press

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "ZComDef.h"
#include "OSAL.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "MT_SYS.h"

#include "nwk_util.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"
#include "zcl_ezmode.h"
#include "zcl_diagnostic.h"
#include "zcl_electrical_measurement.h"
#include "zcl_openevse.h"

#include "onboard.h"

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_flash.h"

#if ( defined (ZGP_DEVICE_TARGET) || defined (ZGP_DEVICE_TARGETPLUS) \
      || defined (ZGP_DEVICE_COMBO) || defined (ZGP_DEVICE_COMBO_MIN) )
#include "zgp_translationtable.h"
  #if (SUPPORTED_S_FEATURE(SUPP_ZGP_FEATURE_TRANSLATION_TABLE))
    #define ZGP_AUTO_TT
  #endif
#endif

#if (defined HAL_BOARD_ZLIGHT) || (defined HAL_PWM)
#include "math.h"
#include "hal_timer.h"
#endif

#include "NLMEDE.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

enum evseCmd { EVSE_CMD_NONE, EVSE_CMD_STATE, EVSE_CMD_WIFI, EVSE_CMD_SLEEP, EVSE_CMD_ENABLE,
                  EVSE_CMD_LCDOFF, EVSE_CMD_LCDRGB, EVSE_CMD_LCDTEAL, EVSE_CMD_GETPOWER,
                  EVSE_CMD_GETTEMP, EVSE_CMD_GETENERGY, EVSE_CMD_GETSTATE, EVSE_CMD_GETSETTINGS,
                  EVSE_CMD_SETLIMIT, EVSE_CMD_SETCURRENT };

const char * evseCode[] = { "", "ST", "WF", "FS", "FE",
                            "FB 0", "S0 1", "FB 6", "GG",
                            "GP", "GU", "GS", "GE",
                            "SH", "SC" };

#define POLL_EVSE_PERIOD 200
#define OPENEVSE_BL_NV 0x0401
#define OPENEVSE_LIMIT_NV 0x0402
#define OPENEVSE_L2_VOLTS 2400
#define OPENEVSE_L1_VOLTS 1200

#define OPENEVSE_CMD_TIMEOUT 1500 // expect response in 1500ms

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
byte zclOpenEvse_TaskID;
uint8 zclOpenEvse_seqNum = 0;

/*********************************************************************
 * GLOBAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
afAddrType_t zclOpenEvse_DstAddr;

uint16 bindingInClusters[] =
{
  ZCL_CLUSTER_ID_GEN_ON_OFF
};
#define zclOpenEvse_BINDINGLIST (sizeof(bindingInClusters) / sizeof(bindingInClusters[0]))

uint8 gPermitDuration = 0;    // permit joining default to disabled

devStates_t zclOpenEvse_NwkState = DEV_INIT;

uint8 zclOpenEvse_evseResendCtr = 0;
uint8 zclOpenEvse_evseCmd = EVSE_CMD_NONE;
uint8 zclOpenEvse_lastOnOff = FALSE;

uint8 zclOpenEvse_powerLevel = 0;

uint32 zclOpenEvse_reportPowerMin =  2000; // 2 seconds
uint32 zclOpenEvse_reportPowerMax =  60000; // 1 minute
uint32 zclOpenEvse_reportTempMax =   120000; // 2 minutes
uint32 zclOpenEvse_reportEnergyMax = 180000; // 3 minutes

uint32 zclOpenEvse_reportPowerChangedVolts = 5 * 10.0; // 5 volts
uint32 zclOpenEvse_reportPowerChangedAmps =  1 * 10.0; // 1 amp
uint32 zclOpenEvse_reportPowerChangedWatts = 200 / 10.0; // 200 watts

zclReportCmd_t * zclOpenEvse_reportCmdVolts;
zclReportCmd_t * zclOpenEvse_reportCmdAmps;
zclReportCmd_t * zclOpenEvse_reportCmdWatts;
zclReportCmd_t * zclOpenEvse_reportCmdTemp;
zclReportCmd_t * zclOpenEvse_reportCmdEnergySum;
zclReportCmd_t * zclOpenEvse_reportCmdEnergyDemand;
zclReportCmd_t * zclOpenEvse_reportCmdState;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void zclOpenEvse_BasicResetCB(void);
static void zclOpenEvse_OnOffCB(uint8 cmd);
static void zclOpenEvse_Identify(void);

static void zclOpenEvse_sendPower(void);
static void zclOpenEvse_sendTemp(void);
static void zclOpenEvse_sendEnergy(void);
static void zclOpenEvse_sendState(void);
static void zclOpenEvse_zigbeeReset(void);
static void zclOpenEvse_EVSESetLimit(uint32 limit);
static void zclOpenEvse_EVSEWriteCmd(uint8 command, uint8 numArgs, ...);
static void zclOpenEvse_EVSEResend(void);
static void zclOpenEvse_UARTInit(void);
static void zclOpenEvse_UARTCallback(uint8 port, uint8 event);
static void zclOpenEvse_UARTParse(char * rxData);
static uint8 zclOpenEvse_nibbletohex(uint8 value);
static uint8 zclOpenEvse_hextonibble(uint8 value);
static uint16 zclOpenEvse_u8tohex(uint8 value);
static uint8 zclOpenEvse_hextou8(uint16 value);

// Functions to process ZCL Foundation incoming Command/Response messages
static void zclOpenEvse_ProcessIncomingMsg( zclIncomingMsg_t *msg );
#ifdef ZCL_READ
static uint8 zclOpenEvse_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg );
#endif
#ifdef ZCL_WRITE
static uint8 zclOpenEvse_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg );
#endif
static uint8 zclOpenEvse_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg );
#ifdef ZCL_DISCOVER
static uint8 zclOpenEvse_ProcessInDiscCmdsRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 zclOpenEvse_ProcessInDiscAttrsRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 zclOpenEvse_ProcessInDiscAttrsExtRspCmd( zclIncomingMsg_t *pInMsg );
#endif

/*********************************************************************
 * STATUS STRINGS
 */
/*********************************************************************
 * ZCL General Profile Callback table
 */
static zclGeneral_AppCallbacks_t zclOpenEvse_CmdCallbacks =
{
  zclOpenEvse_BasicResetCB,               // Basic Cluster Reset command
  NULL,                                   // Identify command
#ifdef ZCL_EZMODE
  NULL,                                   // Identify EZ-Mode Invoke command
  NULL,                                   // Identify Update Commission State command
#endif
  NULL,                                   // Identify Trigger Effect command
  NULL,                                   // Identify Query Response command
  zclOpenEvse_OnOffCB,                    // On/Off cluster commands
  NULL,                                   // On/Off cluster enhanced command Off with Effect
  NULL,                                   // On/Off cluster enhanced command On with Recall Global Scene
  NULL,                                   // On/Off cluster enhanced command On with Timed Off
#ifdef ZCL_LEVEL_CTRL
  NULL,                                   // Level Control Move to Level command
  NULL,                                   // Level Control Move command
  NULL,                                   // Level Control Step command
  NULL,                                   // Level Control Stop command
#endif
#ifdef ZCL_GROUPS
  NULL,                                   // Group Response commands
#endif
#ifdef ZCL_SCENES
  NULL,                                   // Scene Store Request command
  NULL,                                   // Scene Recall Request command
  NULL,                                   // Scene Response command
#endif
#ifdef ZCL_ALARMS
  NULL,                                   // Alarm (Response) commands
#endif
#ifdef SE_UK_EXT
  NULL,                                   // Get Event Log command
  NULL,                                   // Publish Event Log command
#endif
  NULL,                                   // RSSI Location command
  NULL                                    // RSSI Location Response command
};

/*********************************************************************
 * @fn          zclOpenEvse_Init
 *
 * @brief       Initialization function for the zclGeneral layer.
 *
 * @param       none
 *
 * @return      none
 */
void zclOpenEvse_Init( byte task_id )
{
  zclOpenEvse_UARTInit();

  zclOpenEvse_TaskID = task_id;

  // Set destination address to indirect
  zclOpenEvse_DstAddr.addrMode = (afAddrMode_t)AddrNotPresent;
  zclOpenEvse_DstAddr.endPoint = 0;
  zclOpenEvse_DstAddr.addr.shortAddr = 0;

  // This app is part of the Home Automation Profile
  zclHA_Init( &zclOpenEvse_SimpleDesc );
  zclHA_Init( &zclOpenEvse_BlSimpleDesc );

  // Register the ZCL General Cluster Library callback functions
  zclGeneral_RegisterCmdCallbacks( OPENEVSE_ENDPOINT, &zclOpenEvse_CmdCallbacks );

  // Register the backlight callback functions
  zclGeneral_RegisterCmdCallbacks( OPENEVSE_ENDPOINT+1, &zclOpenEvse_CmdCallbacks );

  // Register the application's attribute list
  zcl_registerAttrList( OPENEVSE_ENDPOINT, zclOpenEvse_NumAttributes, zclOpenEvse_Attrs );

    // Register the backlight attribute list
  zcl_registerAttrList( OPENEVSE_ENDPOINT+1, zclOpenEvse_BlNumAttributes, zclOpenEvse_BlAttrs );

  // Register the Application to receive the unprocessed Foundation command/response messages
  zcl_registerForMsg( zclOpenEvse_TaskID );
  
#ifdef ZCL_DISCOVER
  // Register the application's command list
  zcl_registerCmdList( OPENEVSE_ENDPOINT, zclCmdsArraySize, zclOpenEvse_Cmds );
#endif

  // Register for all key events - This app will handle all key events
  //RegisterForKeys( zclOpenEvse_TaskID );

#ifdef ZGP_AUTO_TT
  zgpTranslationTable_RegisterEP ( &zclOpenEvse_SimpleDesc );
#endif

  // Create the Voltage report command
  zclOpenEvse_reportCmdVolts = (zclReportCmd_t *)osal_mem_alloc( sizeof( zclReportCmd_t ) +
                 ( 1 * sizeof( zclReport_t ) ) );
  if ( zclOpenEvse_reportCmdVolts != NULL )
  {
    zclOpenEvse_reportCmdVolts->numAttr = 1;

    // Set up the first attribute
    zclOpenEvse_reportCmdVolts->attrList[0].attrID = ATTRID_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE;
    zclOpenEvse_reportCmdVolts->attrList[0].dataType = ZCL_DATATYPE_UINT16;
    zclOpenEvse_reportCmdVolts->attrList[0].attrData = (uint8 *)&zclOpenEvse_voltsScaled;
  }

  // Create the Amperage report command
  zclOpenEvse_reportCmdAmps = (zclReportCmd_t *)osal_mem_alloc( sizeof( zclReportCmd_t ) +
                 ( 1 * sizeof( zclReport_t ) ) );
  if ( zclOpenEvse_reportCmdAmps != NULL )
  {
    zclOpenEvse_reportCmdAmps->numAttr = 1;

    // Set up the first attribute
    zclOpenEvse_reportCmdAmps->attrList[0].attrID = ATTRID_ELECTRICAL_MEASUREMENT_RMS_CURRENT;
    zclOpenEvse_reportCmdAmps->attrList[0].dataType = ZCL_DATATYPE_UINT16;
    zclOpenEvse_reportCmdAmps->attrList[0].attrData = (uint8 *)&zclOpenEvse_ampsScaled;
  }

  // Create the Watts report command
  zclOpenEvse_reportCmdWatts = (zclReportCmd_t *)osal_mem_alloc( sizeof( zclReportCmd_t ) +
                 ( 1 * sizeof( zclReport_t ) ) );
  if ( zclOpenEvse_reportCmdWatts != NULL )
  {
    zclOpenEvse_reportCmdWatts->numAttr = 1;

    // Set up the first attribute
    zclOpenEvse_reportCmdWatts->attrList[0].attrID = ATTRID_ELECTRICAL_MEASUREMENT_ACTIVE_POWER;
    zclOpenEvse_reportCmdWatts->attrList[0].dataType = ZCL_DATATYPE_INT16;
    zclOpenEvse_reportCmdWatts->attrList[0].attrData = (uint8 *)&zclOpenEvse_wattsScaled;
  }

  // Create the Temperature report command
  zclOpenEvse_reportCmdTemp = (zclReportCmd_t *)osal_mem_alloc( sizeof( zclReportCmd_t ) +
                 ( 1 * sizeof( zclReport_t ) ) );
  if ( zclOpenEvse_reportCmdTemp != NULL )
  {
    zclOpenEvse_reportCmdTemp->numAttr = 1;

    // Set up the first attribute
    zclOpenEvse_reportCmdTemp->attrList[0].attrID = ATTRID_DEV_TEMP_CURRENT;
    zclOpenEvse_reportCmdTemp->attrList[0].dataType = ZCL_DATATYPE_INT16;
    zclOpenEvse_reportCmdTemp->attrList[0].attrData = (uint8 *)&zclOpenEvse_temperature;
  }

    // Create the Energy Sum report command
  zclOpenEvse_reportCmdEnergySum = (zclReportCmd_t *)osal_mem_alloc( sizeof( zclReportCmd_t ) +
                 ( 1 * sizeof( zclReport_t ) ) );
  if ( zclOpenEvse_reportCmdEnergySum != NULL )
  {
    zclOpenEvse_reportCmdEnergySum->numAttr = 1;

    // Set up the first attribute
    zclOpenEvse_reportCmdEnergySum->attrList[0].attrID = ATTRID_CURRENT_SUM_DELIVERED;
    zclOpenEvse_reportCmdEnergySum->attrList[0].dataType = ZCL_DATATYPE_UINT48;
    zclOpenEvse_reportCmdEnergySum->attrList[0].attrData = (uint8 *)&zclOpenEvse_energySum;
  }
  
  // Create the Energy Demand report command
  zclOpenEvse_reportCmdEnergyDemand = (zclReportCmd_t *)osal_mem_alloc( sizeof( zclReportCmd_t ) +
                 ( 1 * sizeof( zclReport_t ) ) );
  if ( zclOpenEvse_reportCmdEnergyDemand != NULL )
  {
    zclOpenEvse_reportCmdEnergyDemand->numAttr = 1;

    // Set up the first attribute
    zclOpenEvse_reportCmdEnergyDemand->attrList[0].attrID = ATTRID_CURRENT_DEMAND_DELIVERED;
    zclOpenEvse_reportCmdEnergyDemand->attrList[0].dataType = ZCL_DATATYPE_UINT24;
    zclOpenEvse_reportCmdEnergyDemand->attrList[0].attrData = (uint8 *)&zclOpenEvse_energyDemand;
  }

  // Create the State report command
  zclOpenEvse_reportCmdState = (zclReportCmd_t *)osal_mem_alloc( sizeof( zclReportCmd_t ) +
                 ( 1 * sizeof( zclReport_t ) ) );
  if ( zclOpenEvse_reportCmdState != NULL )
  {
    zclOpenEvse_reportCmdState->numAttr = 1;

    // Set up the first attribute
    zclOpenEvse_reportCmdState->attrList[0].attrID = ATTRID_IOV_BASIC_PRESENT_VALUE;
    zclOpenEvse_reportCmdState->attrList[0].dataType = ZCL_DATATYPE_UINT16;
    zclOpenEvse_reportCmdState->attrList[0].attrData = (uint8 *)&zclOpenEvse_state;
  }

  // Restore backlight setting
  zcl_nv_item_init( OPENEVSE_BL_NV, sizeof(zclOpenEvse_backlight), &zclOpenEvse_backlight );
  zcl_nv_read( OPENEVSE_BL_NV, 0, sizeof(zclOpenEvse_backlight), &zclOpenEvse_backlight );
  zcl_nv_item_init( OPENEVSE_LIMIT_NV, sizeof(zclOpenEvse_energyLimit), &zclOpenEvse_energyLimit );
  zcl_nv_read( OPENEVSE_LIMIT_NV, 0, sizeof(zclOpenEvse_energyLimit), &zclOpenEvse_energyLimit );

  osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_POLL_EVSE_EVT, 6000 ); // 6 seconds for EVSE to boot and detect level
}


/*********************************************************************
 * @fn          zclOpenEvse_event_loop
 *
 * @brief       Event Loop Processor for zclGeneral.
 *
 * @param       none
 *
 * @return      none
 */
uint16 zclOpenEvse_event_loop( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;

  static uint16 lastVolts = 0, lastAmps = 0;
  static int16 lastWatts = 0;
  
  (void)task_id;  // Intentionally unreferenced parameter

  if ( events & SYS_EVENT_MSG )
  {
    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( zclOpenEvse_TaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
        case ZCL_INCOMING_MSG:
          // Incoming ZCL Foundation command/response messages
          zclOpenEvse_ProcessIncomingMsg( (zclIncomingMsg_t *)MSGpkt );
          break;

        case KEY_CHANGE:
          break;

        case ZDO_STATE_CHANGE:
          zclOpenEvse_NwkState = (devStates_t)(MSGpkt->hdr.status);

          // now on the network
          if ( (zclOpenEvse_NwkState == DEV_ZB_COORD) ||
               (zclOpenEvse_NwkState == DEV_ROUTER)   ||
               (zclOpenEvse_NwkState == DEV_END_DEVICE) )
          {
            zclOpenEvse_Identify();
          }
          break;

        default:
          break;
      }

      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }
  
  if ( (events & OPENEVSE_CMD_TIMEOUT_EVT) )
  {
    if (zclOpenEvse_evseCmd != EVSE_CMD_NONE)
    {
      zclOpenEvse_EVSEResend();  // Resend if command didn't get a response
    }
    return (events ^ OPENEVSE_CMD_TIMEOUT_EVT);
  }

  if ( (events & OPENEVSE_IDENTIFY_EVT) )
  {
    static uint8 identState = 0;

    if (zclOpenEvse_evseCmd != EVSE_CMD_NONE)
    {
      return events; // If last command not complete, postpone this
    }

    if (zclOpenEvse_IdentifyTime == 0)
    {
      if (zclOpenEvse_backlight == LIGHT_ON)
      {
        zclOpenEvse_EVSEWriteCmd(EVSE_CMD_LCDRGB, 0);
      }
      else
      {
        zclOpenEvse_EVSEWriteCmd(EVSE_CMD_LCDOFF, 0);
      }
    }
    else
    {
      if (identState & 1) // On odd counts turn LED on
      {
        zclOpenEvse_EVSEWriteCmd(EVSE_CMD_LCDOFF, 0);
        zclOpenEvse_IdentifyTime--;
      }
      else
      {
        zclOpenEvse_EVSEWriteCmd(EVSE_CMD_LCDTEAL, 0);
      }
      identState = !identState;
      osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_IDENTIFY_EVT, 500 );
    }
    return ( events ^ OPENEVSE_IDENTIFY_EVT );
  }
  
  if (events & OPENEVSE_POLL_EVSE_EVT)
  {
    static uint8 pollNumber = 0;
    static uint8 firstTime = TRUE;
    static uint8 lastBacklight = TRUE;
    static uint32 lastLimit = 0;

    if (zclOpenEvse_evseCmd != EVSE_CMD_NONE)
    {
      return events; // If last command not complete, postpone this
    }

    if (zclOpenEvse_OnOff != zclOpenEvse_lastOnOff)
    {
      zclOpenEvse_lastOnOff = zclOpenEvse_OnOff;
      if (zclOpenEvse_OnOff == LIGHT_ON)
      {
        zclOpenEvse_EVSEWriteCmd(EVSE_CMD_ENABLE, 0);
      }
      else
      {
        zclOpenEvse_EVSEWriteCmd(EVSE_CMD_SLEEP, 0);
      }
      osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_POLL_EVSE_EVT, POLL_EVSE_PERIOD );
      return ( events ^ OPENEVSE_POLL_EVSE_EVT );
    }

    if (zclOpenEvse_backlight != lastBacklight)
    {
      lastBacklight = zclOpenEvse_backlight;
      if (zclOpenEvse_backlight == LIGHT_ON)
      {
        zclOpenEvse_EVSEWriteCmd(EVSE_CMD_LCDRGB, 0);
      }
      else
      {
        zclOpenEvse_EVSEWriteCmd(EVSE_CMD_LCDOFF, 0);
      }
      osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_POLL_EVSE_EVT, POLL_EVSE_PERIOD );
      return ( events ^ OPENEVSE_POLL_EVSE_EVT );
    }

    switch (pollNumber++)
    {
    case 0: // State 0-9 initialization
      zclOpenEvse_EVSEWriteCmd(EVSE_CMD_GETSETTINGS, 0);
      break;
    case 1:
      zclOpenEvse_EVSEWriteCmd(EVSE_CMD_GETSTATE, 0);
      pollNumber = 10; // Go to main loop state
      break;
      
    case 10:// State 10-19 main loop
      zclOpenEvse_EVSEWriteCmd(EVSE_CMD_GETPOWER, 0);
      break;
    case 11:
      zclOpenEvse_EVSEWriteCmd(EVSE_CMD_GETTEMP, 0);
      break;
    case 12:
      zclOpenEvse_EVSEWriteCmd(EVSE_CMD_GETENERGY, 0);
      if (zclOpenEvse_NwkState != DEV_ROUTER)
      {
        firstTime = TRUE;
      }
      if (lastLimit != zclOpenEvse_energyLimit)
      {
        pollNumber = 40; // Go to set limit state
        break;
      }
      else if (firstTime)
      {
        pollNumber = 20; // Go to network init state
        break;
      }
      pollNumber = 10;
      break;

    case 20: // State 20-39 network connected
      if (zclOpenEvse_NwkState != DEV_ROUTER)
      {
        pollNumber = 10; // Return to main loop state
        break;
      }
      break;
      // States 21-29 are a delay after network join
    case 30:
      zclOpenEvse_sendEnergy();
      break;
    case 31:
      lastVolts = zclOpenEvse_voltsScaled;
      lastAmps = zclOpenEvse_ampsScaled;
      lastWatts = zclOpenEvse_wattsScaled;
      zclOpenEvse_sendPower();
      break;
    case 32:
      zclOpenEvse_sendTemp();
      break;
    case 33:
      zclOpenEvse_sendState();
      firstTime = FALSE;
      // Network is configured so start report timers
      osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_GETPOWER_MIN_EVT, zclOpenEvse_reportPowerMin );
      osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_GETPOWER_MAX_EVT, zclOpenEvse_reportPowerMax );
      osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_GETTEMP_MAX_EVT, zclOpenEvse_reportTempMax );
      osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_GETENERGY_MAX_EVT, zclOpenEvse_reportEnergyMax );
      pollNumber = 10;
      break;

    case 40:
      zclOpenEvse_EVSESetLimit(zclOpenEvse_energyLimit);
      // Save to NVRAM
      zcl_nv_write( OPENEVSE_LIMIT_NV, 0, sizeof(zclOpenEvse_energyLimit), &zclOpenEvse_energyLimit );
      lastLimit = zclOpenEvse_energyLimit;
      pollNumber = 10;
      break;
    }
    
    osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_POLL_EVSE_EVT, POLL_EVSE_PERIOD );
    return ( events ^ OPENEVSE_POLL_EVSE_EVT );
  }
  if (events & OPENEVSE_BACKLIGHT_OFF_EVT)
  {
    if (zclOpenEvse_evseCmd != EVSE_CMD_NONE)
    {
      return events; // If last command not complete, postpone this
    }
 
    zclOpenEvse_EVSEWriteCmd(EVSE_CMD_LCDOFF, 0);

    return ( events ^ OPENEVSE_BACKLIGHT_OFF_EVT );
  }
  if ( events & OPENEVSE_GETPOWER_MIN_EVT)
  {
    if ( (abs(lastVolts - zclOpenEvse_voltsScaled) > zclOpenEvse_reportPowerChangedVolts) ||
         (abs(lastAmps - zclOpenEvse_ampsScaled) > zclOpenEvse_reportPowerChangedAmps) ||
         (abs(lastWatts - zclOpenEvse_wattsScaled) > zclOpenEvse_reportPowerChangedWatts) )
    {
      lastVolts = zclOpenEvse_voltsScaled;
      lastAmps = zclOpenEvse_ampsScaled;
      lastWatts = zclOpenEvse_wattsScaled;

      zclOpenEvse_sendPower();
    }      
    osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_GETPOWER_MIN_EVT, zclOpenEvse_reportPowerMin );
    return ( events ^ OPENEVSE_GETPOWER_MIN_EVT );
  }
  if ( events & OPENEVSE_GETPOWER_MAX_EVT)
  {
    zclOpenEvse_sendPower(); // This restarts the timer
    return ( events ^ OPENEVSE_GETPOWER_MAX_EVT );
  }

  if ( events & OPENEVSE_GETTEMP_MAX_EVT)
  {
    zclOpenEvse_sendTemp(); // This restarts the timer
    return ( events ^ OPENEVSE_GETTEMP_MAX_EVT );
  }

  if ( events & OPENEVSE_GETENERGY_MAX_EVT)
  {
    zclOpenEvse_sendEnergy();
    osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_GETENERGY_MAX_EVT, zclOpenEvse_reportEnergyMax );
    return ( events ^ OPENEVSE_GETENERGY_MAX_EVT );
  }

  // Discard unknown events
  return 0;
}


/*********************************************************************
 * @fn      zclOpenEvse_BasicResetCB
 *
 * @brief   Callback from the ZCL General Cluster Library
 *          to set all the Basic Cluster attributes to default values.
 *
 * @param   none
 *
 * @return  none
 */
static void zclOpenEvse_BasicResetCB( void )
{
  NLME_LeaveReq_t leaveReq;
  // Set every field to 0
  osal_memset( &leaveReq, 0, sizeof( NLME_LeaveReq_t ) );

  // This will enable the device to rejoin the network after reset.
  leaveReq.rejoin = TRUE;

  // Set the NV startup option to force a "new" join.
  zgWriteStartupOptions( ZG_STARTUP_SET, ZCD_STARTOPT_DEFAULT_NETWORK_STATE );

  // Leave the network, and reset afterwards
  if ( NLME_LeaveReq( &leaveReq ) != ZSuccess )
  {
    // Couldn't send out leave; prepare to reset anyway
    ZDApp_LeaveReset( FALSE );
  }
}

/*********************************************************************
 * @fn      zclOpenEvse_OnOffCB
 *
 * @brief   Callback from the ZCL General Cluster Library when
 *          it received an On/Off Command for this application.
 *
 * @param   cmd - COMMAND_ON, COMMAND_OFF or COMMAND_TOGGLE
 *
 * @return  none
 */
static void zclOpenEvse_OnOffCB( uint8 cmd )
{
  afIncomingMSGPacket_t *pPtr = zcl_getRawAFMsg();

  if (pPtr->endPoint == OPENEVSE_ENDPOINT)
  {
    // Turn on the power
    if ( cmd == COMMAND_ON )
    {
      zclOpenEvse_OnOff = LIGHT_ON;
    }
    // Turn off the power
    else if ( cmd == COMMAND_OFF )
    {
      zclOpenEvse_OnOff = LIGHT_OFF;
    }
    // Toggle the power
    else if ( cmd == COMMAND_TOGGLE )
    {
      if ( zclOpenEvse_OnOff == LIGHT_OFF )
      {
        zclOpenEvse_OnOff = LIGHT_ON;
      }
      else
      {
        zclOpenEvse_OnOff = LIGHT_OFF;
      }
    }
  }
  else
  {
    // Turn on the backlight
    if ( cmd == COMMAND_ON )
    {
      zclOpenEvse_backlight = LIGHT_ON;
    }
    // Turn off the backlight
    else if ( cmd == COMMAND_OFF )
    {
      zclOpenEvse_backlight = LIGHT_OFF;
    }
    // Toggle the backlight
    else if ( cmd == COMMAND_TOGGLE )
    {
      if ( zclOpenEvse_backlight == LIGHT_OFF )
      {
        zclOpenEvse_backlight = LIGHT_ON;
      }
      else
      {
        zclOpenEvse_backlight = LIGHT_OFF;
      }
    }
    
    // save to NVRAM
    zcl_nv_write( OPENEVSE_BL_NV, 0, sizeof(zclOpenEvse_backlight), &zclOpenEvse_backlight );
  }
}

void zclOpenEvse_Identify(void)
{
  zclOpenEvse_IdentifyTime = 5;
  osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_IDENTIFY_EVT, 500 );
}

/******************************************************************************
 *
 *  Functions for processing ZCL Foundation incoming Command/Response messages
 *
 *****************************************************************************/

/*********************************************************************
 * @fn      zclOpenEvse_ProcessIncomingMsg
 *
 * @brief   Process ZCL Foundation incoming message
 *
 * @param   pInMsg - pointer to the received message
 *
 * @return  none
 */
static void zclOpenEvse_ProcessIncomingMsg( zclIncomingMsg_t *pInMsg )
{
  switch ( pInMsg->zclHdr.commandID )
  {
#ifdef ZCL_READ
    case ZCL_CMD_READ_RSP:
      zclOpenEvse_ProcessInReadRspCmd( pInMsg );
      break;
#endif
#ifdef ZCL_WRITE
    case ZCL_CMD_WRITE_RSP:
      zclOpenEvse_ProcessInWriteRspCmd( pInMsg );
      break;
#endif
#ifdef ZCL_REPORT
    // Attribute Reporting implementation should be added here
    case ZCL_CMD_CONFIG_REPORT:
      // zclOpenEvse_ProcessInConfigReportCmd( pInMsg );
      break;

    case ZCL_CMD_CONFIG_REPORT_RSP:
      // zclOpenEvse_ProcessInConfigReportRspCmd( pInMsg );
      break;

    case ZCL_CMD_READ_REPORT_CFG:
      // zclOpenEvse_ProcessInReadReportCfgCmd( pInMsg );
      break;

    case ZCL_CMD_READ_REPORT_CFG_RSP:
      // zclOpenEvse_ProcessInReadReportCfgRspCmd( pInMsg );
      break;

    case ZCL_CMD_REPORT:
      // zclOpenEvse_ProcessInReportCmd( pInMsg );
      break;
#endif
    case ZCL_CMD_DEFAULT_RSP:
      zclOpenEvse_ProcessInDefaultRspCmd( pInMsg );
      break;
#ifdef ZCL_DISCOVER
    case ZCL_CMD_DISCOVER_CMDS_RECEIVED_RSP:
      zclOpenEvse_ProcessInDiscCmdsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_CMDS_GEN_RSP:
      zclOpenEvse_ProcessInDiscCmdsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_ATTRS_RSP:
      zclOpenEvse_ProcessInDiscAttrsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_ATTRS_EXT_RSP:
      zclOpenEvse_ProcessInDiscAttrsExtRspCmd( pInMsg );
      break;
#endif
    default:
      break;
  }

  if ( pInMsg->attrCmd )
    osal_mem_free( pInMsg->attrCmd );
}

#ifdef ZCL_READ
/*********************************************************************
 * @fn      zclOpenEvse_ProcessInReadRspCmd
 *
 * @brief   Process the "Profile" Read Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclOpenEvse_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclReadRspCmd_t *readRspCmd;
  uint8 i;

  readRspCmd = (zclReadRspCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < readRspCmd->numAttr; i++)
  {
    // Notify the originator of the results of the original read attributes
    // attempt and, for each successfull request, the value of the requested
    // attribute
  }

  return ( TRUE );
}
#endif // ZCL_READ

#ifdef ZCL_WRITE
/*********************************************************************
 * @fn      zclOpenEvse_ProcessInWriteRspCmd
 *
 * @brief   Process the "Profile" Write Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclOpenEvse_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclWriteRspCmd_t *writeRspCmd;
  uint8 i;

  writeRspCmd = (zclWriteRspCmd_t *)pInMsg->attrCmd;
  for ( i = 0; i < writeRspCmd->numAttr; i++ )
  {
    // Notify the device of the results of the its original write attributes
    // command.
  }

  return ( TRUE );
}
#endif // ZCL_WRITE

/*********************************************************************
 * @fn      zclOpenEvse_ProcessInDefaultRspCmd
 *
 * @brief   Process the "Profile" Default Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclOpenEvse_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg )
{
  // zclDefaultRspCmd_t *defaultRspCmd = (zclDefaultRspCmd_t *)pInMsg->attrCmd;

  // Device is notified of the Default Response command.
  (void)pInMsg;

  return ( TRUE );
}

#ifdef ZCL_DISCOVER
/*********************************************************************
 * @fn      zclOpenEvse_ProcessInDiscCmdsRspCmd
 *
 * @brief   Process the Discover Commands Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclOpenEvse_ProcessInDiscCmdsRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverCmdsCmdRsp_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverCmdsCmdRsp_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numCmd; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return ( TRUE );
}

/*********************************************************************
 * @fn      zclOpenEvse_ProcessInDiscAttrsRspCmd
 *
 * @brief   Process the "Profile" Discover Attributes Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclOpenEvse_ProcessInDiscAttrsRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverAttrsRspCmd_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverAttrsRspCmd_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return ( TRUE );
}

/*********************************************************************
 * @fn      zclOpenEvse_ProcessInDiscAttrsExtRspCmd
 *
 * @brief   Process the "Profile" Discover Attributes Extended Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclOpenEvse_ProcessInDiscAttrsExtRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverAttrsExtRsp_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverAttrsExtRsp_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return ( TRUE );
}
#endif // ZCL_DISCOVER


void zclOpenEvse_sendPower(void)
{
  // Restart max timer because we just sent
  osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_GETPOWER_MAX_EVT, zclOpenEvse_reportPowerMax );

  zcl_SendReportCmd( OPENEVSE_ENDPOINT, &zclOpenEvse_DstAddr,
                     ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT, zclOpenEvse_reportCmdVolts,
                     ZCL_FRAME_SERVER_CLIENT_DIR, 0, zclOpenEvse_seqNum++ ); 

  zcl_SendReportCmd( OPENEVSE_ENDPOINT, &zclOpenEvse_DstAddr,
                     ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT, zclOpenEvse_reportCmdAmps,
                     ZCL_FRAME_SERVER_CLIENT_DIR, 0, zclOpenEvse_seqNum++ ); 

  zcl_SendReportCmd( OPENEVSE_ENDPOINT, &zclOpenEvse_DstAddr,
                     ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT, zclOpenEvse_reportCmdWatts,
                     ZCL_FRAME_SERVER_CLIENT_DIR, 0, zclOpenEvse_seqNum++ ); 
}

void zclOpenEvse_sendTemp(void)
{
  osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_GETTEMP_MAX_EVT, zclOpenEvse_reportTempMax );

  zcl_SendReportCmd( OPENEVSE_ENDPOINT, &zclOpenEvse_DstAddr,
                     ZCL_CLUSTER_ID_GEN_DEVICE_TEMP_CONFIG, zclOpenEvse_reportCmdTemp,
                     ZCL_FRAME_SERVER_CLIENT_DIR, 0, zclOpenEvse_seqNum++ ); 
}

void zclOpenEvse_sendEnergy(void)
{
  osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_GETENERGY_MAX_EVT, zclOpenEvse_reportEnergyMax );
  
  zcl_SendReportCmd( OPENEVSE_ENDPOINT, &zclOpenEvse_DstAddr,
                     ZCL_CLUSTER_ID_SE_METERING, zclOpenEvse_reportCmdEnergySum,
                     ZCL_FRAME_SERVER_CLIENT_DIR, 0, zclOpenEvse_seqNum++ ); 

  zcl_SendReportCmd( OPENEVSE_ENDPOINT, &zclOpenEvse_DstAddr,
                     ZCL_CLUSTER_ID_SE_METERING, zclOpenEvse_reportCmdEnergyDemand,
                     ZCL_FRAME_SERVER_CLIENT_DIR, 0, zclOpenEvse_seqNum++ ); 
}

void zclOpenEvse_sendState(void)
{
  zcl_SendReportCmd( OPENEVSE_ENDPOINT, &zclOpenEvse_DstAddr,
                     ZCL_CLUSTER_ID_GEN_MULTISTATE_INPUT_BASIC, zclOpenEvse_reportCmdState,
                     ZCL_FRAME_SERVER_CLIENT_DIR, 0, zclOpenEvse_seqNum++ ); 
}

void zclOpenEvse_zigbeeReset(void)
{
  int i;
  
  for (i = HAL_NV_PAGE_BEG; i <= (HAL_NV_PAGE_BEG + HAL_NV_PAGE_CNT); i++)
  {
    HalFlashErase(i);
  }
  Onboard_soft_reset();
}

void zclOpenEvse_EVSESetLimit(uint32 limit)
{
  if (limit == 0xFFFFFF)
  {
    limit = 0;
  }
  zclOpenEvse_EVSEWriteCmd(EVSE_CMD_SETLIMIT, 1, (int32)limit);
}

void zclOpenEvse_EVSEWriteCmd(uint8 command, uint8 numArgs, ...)
{
  static char string[16+5] = "$";
  int strLen = 4;
  unsigned char chk = 0;
  va_list valist;
  int i;

  zclOpenEvse_evseCmd = command;
  
  strcpy(&string[1], (const char *)evseCode[command]);

  va_start(valist, numArgs);

  for (i = 0; i < numArgs && i < 3; i++)
  {
    sprintf(string + strlen(string), " %ld", va_arg(valist, int32));
  }

  va_end(valist);

  for (strLen = 0; strLen < strlen(string); strLen ++)
  {
    chk ^= string[strLen];
  }
  string[strLen++] = '^';
  *((uint16 *)(&string[strLen++])) = zclOpenEvse_u8tohex(chk);
  strLen++;
  string[strLen++] = '\r';
  string[strLen++] = 0;
  
  HalUARTWrite(HAL_UART_PORT_0, (uint8 *)string, strLen);
  osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_CMD_TIMEOUT_EVT, OPENEVSE_CMD_TIMEOUT );
}

inline void zclOpenEvse_EVSEResend(void)
{
  if (zclOpenEvse_evseResendCtr++ > 3)
  {
    zclOpenEvse_evseCmd = EVSE_CMD_NONE;
    zclOpenEvse_evseResendCtr = 0;
    return;
  }

  HalUARTWrite(HAL_UART_PORT_0, (uint8 *)"\r", 1);
  zclOpenEvse_EVSEWriteCmd(zclOpenEvse_evseCmd, 0);
}

void zclOpenEvse_UARTInit(void)
{
  halUARTCfg_t uartConfig;

  /* UART Configuration */
  uartConfig.configured           = TRUE;
  uartConfig.baudRate             = HAL_UART_BR_115200;
  uartConfig.flowControl          = HAL_UART_FLOW_OFF;
  uartConfig.flowControlThreshold = HAL_UART_DMA_RX_MAX >> 1;
  uartConfig.rx.maxBufSize        = HAL_UART_DMA_RX_MAX;
  uartConfig.tx.maxBufSize        = HAL_UART_DMA_RX_MAX;
  uartConfig.idleTimeout          = 50;
  uartConfig.intEnable            = TRUE;
  uartConfig.callBackFunc         = zclOpenEvse_UARTCallback;

  /* Start UART */
  HalUARTOpen(HAL_UART_PORT_0, &uartConfig);
}

int8 rxIndex = -1;
uint8 rxData[34];

void zclOpenEvse_UARTCallback(uint8 port, uint8 event)
{
  static uint8 waitSoc = TRUE;

  uint8 ch;
  while (Hal_UART_RxBufLen(port))
  {
    HalUARTRead (port, &ch, 1);
    switch (ch)
    {
    case '$':
      rxIndex = 0;
      waitSoc = FALSE;
      break;
    case '\r':
      if (waitSoc)
      {
        break;
      }
      rxData[rxIndex] = 0;
      waitSoc = TRUE;
      zclOpenEvse_UARTParse((char *)rxData);
      break;
    default:
      if (waitSoc || rxIndex >= 64)
      {
        break;
      }
      rxData[rxIndex++] = ch;
      break;
    }
  }
}


void zclOpenEvse_UARTParse(char * rxData)
{
  unsigned char chk = '$', i;

  for (i = 0; i < strlen(rxData)-3; i ++)
  {
    chk ^= (uint8)rxData[i];
  }

  if (chk != zclOpenEvse_hextou8(*((uint16 *)&rxData[i+1]))) // If bad checksum, resend
  {
    zclOpenEvse_EVSEResend();
    return;
  }

  if (!strncmp((const char *)rxData, evseCode[EVSE_CMD_STATE], 2)) // Asynchronous state update
  {
    char * valid = NULL;
    uint8 state = strtol((const char *)&rxData[3], &valid, 16);
    if (valid)
    {
      if (zclOpenEvse_backlight == LIGHT_OFF) // Turn backlight back off after change of state
      {
        osal_start_timerEx( zclOpenEvse_TaskID, OPENEVSE_BACKLIGHT_OFF_EVT, 5000 );
      }
      if (state == 0xFE)
      {
        zclOpenEvse_OnOff = LIGHT_OFF;
      }
      else
      {
        zclOpenEvse_OnOff = LIGHT_ON;
      }
      zclOpenEvse_lastOnOff = zclOpenEvse_OnOff;
      zclOpenEvse_state = state;
      zclOpenEvse_sendState();
    }
    return;
  }
  else if (!strncmp((const char *)rxData, evseCode[EVSE_CMD_WIFI], 2)) // Asynchronous wifi update
  {
    zclOpenEvse_zigbeeReset();
    return;
  } else if (strncmp((const char *)rxData, "OK", 2)) // If not OK resend
  {
    zclOpenEvse_EVSEResend();
    return;
  }
  
  switch (zclOpenEvse_evseCmd)
  {
  case EVSE_CMD_GETPOWER:
    {
      char * amps = strtok(&rxData[3], " ");
      char * volts = strtok(NULL, " ");
      if (!amps || !volts)
      {
        zclOpenEvse_EVSEResend();
        return;
      }
      if (atol(volts) != -1)
      {
        zclOpenEvse_voltsScaled = (uint16) (atol(volts) * 0.01);
      }
      else
      {
        zclOpenEvse_voltsScaled = (zclOpenEvse_powerLevel == 2) ? OPENEVSE_L2_VOLTS : OPENEVSE_L1_VOLTS;
      }

      if (atol(amps) != -1)
      {
        zclOpenEvse_ampsScaled = (uint16) (atol(amps) * 0.01);
      }
      zclOpenEvse_wattsScaled = (int16) ((float)zclOpenEvse_voltsScaled * (float)zclOpenEvse_ampsScaled * 0.001);
    }
    break;
  case EVSE_CMD_GETTEMP:
    {
      char * ds3231 = strtok(&rxData[3], " ");
      char * mcp9808 = strtok(NULL, " ");
      char * tmp007 = strtok(NULL, " ");
      if (!ds3231 || !mcp9808 || !tmp007)
      {
        zclOpenEvse_EVSEResend();
        return;
      }
      zclOpenEvse_temperature = (int16) (atoi(ds3231) * (1.0 / 10)); // Tenths of degree C to degrees C
    }
    break;
  case EVSE_CMD_GETENERGY:
    {
      char * wattSecs = strtok(&rxData[3], " ");
      char * wattAcc = strtok(NULL, " ");
      if (!wattSecs || !wattAcc)
      {
        zclOpenEvse_EVSEResend();
        return;
      }
      zclOpenEvse_energyDemand = (uint32) (atol(wattSecs) * (1.0 / 3600)); // Convert watt-seconds to watt-hours
      *((uint32 *)&zclOpenEvse_energySum) = (uint32) atol(wattAcc); // Already in watt-hours
    }
    break;
  case EVSE_CMD_GETSTATE:
    {
      char * valid = NULL;
      uint8 state = strtol((const char *)&rxData[3], &valid, 10);
      if (valid)
      {
        zclOpenEvse_state = state;
      }
    }
    break;
  case EVSE_CMD_GETSETTINGS:
    {
      char * amps = strtok(&rxData[3], " ");
      char * flags = strtok(NULL, " ");
      if (!amps || !flags)
      {
        zclOpenEvse_EVSEResend();
        return;
      }
      
      zclOpenEvse_powerLevel = (atoi(flags) & 1) ? 2 : 1; // If bit 0 is set, power level is 2
    }
    break;
  }

  zclOpenEvse_evseCmd = EVSE_CMD_NONE;
  zclOpenEvse_evseResendCtr = 0;
  osal_stop_timerEx( zclOpenEvse_TaskID, OPENEVSE_CMD_TIMEOUT_EVT );
}

// converts 4-bit nibble to ascii hex
uint8 zclOpenEvse_nibbletohex(uint8 value)
{
    if (value >= 10) return value - 10 + 'A';
    return value + '0';
}

// converts ascii character to 4-bit nibble
uint8 zclOpenEvse_hextonibble(uint8 value)
{
    if (value >= 'A') return value + 10 - 'A';
    return value - '0';
}

// returns value as 2 ascii characters in a 16-bit int
uint16 zclOpenEvse_u8tohex(uint8 value)
{
    uint16 hexdigits;

    uint8 lodigit = (value >> 4);
    uint8 hidigit = (value & 0x0F);
    hexdigits = zclOpenEvse_nibbletohex(lodigit);
    hexdigits |= (zclOpenEvse_nibbletohex(hidigit) << 8);

    return hexdigits;
}

// value is 2 ascii characters in a 16-bit int
uint8 zclOpenEvse_hextou8(uint16 value)
{
  uint8 chk;
  uint8 lodigit = zclOpenEvse_hextonibble(value >> 8);
  uint8 hidigit = zclOpenEvse_hextonibble(value & 0xFF);

  chk = lodigit;
  chk |= (hidigit << 4);

  return chk;
}
/****************************************************************************
****************************************************************************/


