/**************************************************************************************************
  Filename:       zcl_openevse.h

  Description:    This file contains the Zigbee Cluster Library Home
                  Automation for OpenEVSE.


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
  PROVIDED “AS IS” WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
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

#ifndef ZCL_OPENEVSE_H
#define ZCL_OPENEVSE_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "zcl.h"

/*********************************************************************
 * CONSTANTS
 */
#define OPENEVSE_ENDPOINT            8

#define LIGHT_OFF                       0x00
#define LIGHT_ON                        0x01

// Application Events
#define OPENEVSE_POLL_CONTROL_TIMEOUT_EVT  0x0001
#define OPENEVSE_POLL_EVSE_EVT             0x0002
#define OPENEVSE_IDENTIFY_EVT              0x0004
#define OPENEVSE_BACKLIGHT_OFF_EVT         0x0008
#define OPENEVSE_GETPOWER_MIN_EVT          0x0010
#define OPENEVSE_GETPOWER_MAX_EVT          0x0020
#define OPENEVSE_GETTEMP_MAX_EVT           0x0040
#define OPENEVSE_GETENERGY_MAX_EVT         0x0080
#define OPENEVSE_CMD_TIMEOUT_EVT           0x0100
  
  // Application Display Modes
#define LIGHT_MAINMODE      0x00
#define LIGHT_HELPMODE      0x01

#define ATTRID_CURRENT_SUM_DELIVERED 0x0000
#define ATTRID_CURRENT_DEMAND_DELIVERED 0x0600
#define ATTRID_CURRENT_DEMAND_LIMIT 0x0601
  
/*********************************************************************
 * MACROS
 */
/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * VARIABLES
 */
extern SimpleDescriptionFormat_t zclOpenEvse_SimpleDesc;
extern SimpleDescriptionFormat_t zclOpenEvse_BlSimpleDesc;

extern CONST zclCommandRec_t zclOpenEvse_Cmds[];

extern CONST uint8 zclCmdsArraySize;

// attribute list
extern CONST zclAttrRec_t zclOpenEvse_Attrs[];
extern CONST zclAttrRec_t zclOpenEvse_BlAttrs[];
extern CONST uint8 zclOpenEvse_NumAttributes;
extern CONST uint8 zclOpenEvse_BlNumAttributes;

// Identify attributes
extern uint16 zclOpenEvse_IdentifyTime;
extern uint8  zclOpenEvse_IdentifyCommissionState;

// OnOff attributes
extern uint8  zclOpenEvse_OnOff;
extern uint8  zclOpenEvse_backlight;

// Device Temperature Configuration attributes
extern int16  zclOpenEvse_temperature;

// Multistate attributes
extern uint16 zclOpenEvse_state;

// Metering attributes
extern uint8 zclOpenEvse_energySum[6];
extern uint32 zclOpenEvse_energyDemand;
extern uint32 zclOpenEvse_energyLimit;

// Electrical Measurement attributes
extern uint32 zclOpenEvse_elecMeasType;
extern uint16 zclOpenEvse_voltsScaled;
extern uint16 zclOpenEvse_ampsScaled;
extern int16 zclOpenEvse_wattsScaled;
extern uint16 zclOpenEvse_elecMeasMultiplier;
extern uint16 zclOpenEvse_elecMeasDivisor;

/*********************************************************************
 * FUNCTIONS
 */

 /*
  * Initialization for the task
  */
extern void zclOpenEvse_Init( byte task_id );

/*
 *  Event Process for the task
 */
extern UINT16 zclOpenEvse_event_loop( byte task_id, UINT16 events );


/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ZCL_OPENEVSE_H */
