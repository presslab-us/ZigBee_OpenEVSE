/**************************************************************************************************
  Filename:       zcl_openevse_data.c

  Description:    Zigbee Cluster Library - device data for OpenEVSE.


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
#include "ZComDef.h"
#include "OSAL.h"
#include "AF.h"
#include "ZDConfig.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"
#include "zcl_ezmode.h"
#include "zcl_poll_control.h"
#include "zcl_electrical_measurement.h"
#include "zcl_meter_identification.h"
#include "zcl_appliance_identification.h"
#include "zcl_appliance_events_alerts.h"
#include "zcl_power_profile.h"
#include "zcl_appliance_control.h"
#include "zcl_appliance_statistics.h"
#include "zcl_hvac.h"

#include "zcl_openevse.h"

/*********************************************************************
 * CONSTANTS
 */

#define OPENEVSE_DEVICE_VERSION     0
#define OPENEVSE_FLAGS              0

#define OPENEVSE_HWVERSION          1
#define OPENEVSE_ZCLVERSION         1

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

// Basic Cluster
const uint8 zclOpenEvse_HWRevision = OPENEVSE_HWVERSION;
const uint8 zclOpenEvse_ZCLVersion = OPENEVSE_ZCLVERSION;
const uint8 zclOpenEvse_ManufacturerName[] = { 8, 'P','r','e','s','s','l','a','b' };
const uint8 zclOpenEvse_ModelId[] = { 16, 'P','L','-','E','V','S','E','1',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 zclOpenEvse_DateCode[] = { 16, '2','0','1','5','1','2','2','3',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 zclOpenEvse_PowerSource = POWER_SOURCE_MAINS_1_PHASE;

uint8 zclOpenEvse_LocationDescription[17] = { 16, ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
uint8 zclOpenEvse_PhysicalEnvironment = 0;
uint8 zclOpenEvse_DeviceEnable = DEVICE_ENABLED;

// On/Off Cluster
uint8 zclOpenEvse_OnOff = LIGHT_OFF;
uint8 zclOpenEvse_backlight = LIGHT_ON;

// Device Temperature Configuration attributes
int16 zclOpenEvse_temperature = 20;

// Identify attributes
uint16 zclOpenEvse_IdentifyTime = 0;

// Multistate attributes
uint16 zclOpenEvse_state = 0;

// Metering attributes
uint8 zclOpenEvse_energySum[6] = {0};
uint32 zclOpenEvse_energyDemand = 0;
uint32 zclOpenEvse_energyLimit = 0xFFFFFF;

// Electrical Measurement attributes
uint32 zclOpenEvse_elecMeasType = 0;
uint16 zclOpenEvse_voltsScaled = 0;
uint16 zclOpenEvse_ampsScaled = 0;
int16 zclOpenEvse_wattsScaled = 0;
uint16 zclOpenEvse_elecMeasMultiplier = 10;
uint16 zclOpenEvse_elecMeasDivisor = 1;
uint16 zclOpenEvse_elecMeasWattsMultiplier = 1;
uint16 zclOpenEvse_elecMeasWattsDivisor = 10;

/*********************************************************************
 * ATTRIBUTE DEFINITIONS - Uses REAL cluster IDs
 */
CONST zclAttrRec_t zclOpenEvse_Attrs[] =
{
  // *** General Basic Cluster Attributes ***
  {
    ZCL_CLUSTER_ID_GEN_BASIC,             // Cluster IDs - defined in the foundation (ie. zcl.h)
    {  // Attribute record
      ATTRID_BASIC_HW_VERSION,            // Attribute ID - Found in Cluster Library header (ie. zcl_general.h)
      ZCL_DATATYPE_UINT8,                 // Data Type - found in zcl.h
      ACCESS_CONTROL_READ,                // Variable access control - found in zcl.h
      (void *)&zclOpenEvse_HWRevision  // Pointer to attribute variable
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_ZCL_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_ZCLVersion
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_MANUFACTURER_NAME,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclOpenEvse_ManufacturerName
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_MODEL_ID,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclOpenEvse_ModelId
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_DATE_CODE,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclOpenEvse_DateCode
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_POWER_SOURCE,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_PowerSource
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_LOCATION_DESC,
      ZCL_DATATYPE_CHAR_STR,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)zclOpenEvse_LocationDescription
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_PHYSICAL_ENV,
      ZCL_DATATYPE_UINT8,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclOpenEvse_PhysicalEnvironment
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_DEVICE_ENABLED,
      ZCL_DATATYPE_BOOLEAN,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclOpenEvse_DeviceEnable
    }
  },

  // *** Device Temperature Configuration Cluster Attributes ***
  {
    ZCL_CLUSTER_ID_GEN_DEVICE_TEMP_CONFIG,
    { // Attribute record
      ATTRID_DEV_TEMP_CURRENT,
      ZCL_DATATYPE_INT16,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_temperature
    }
  },

  // *** On/Off Cluster Attributes ***
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    { // Attribute record
      ATTRID_ON_OFF,
      ZCL_DATATYPE_BOOLEAN,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_OnOff
    }
  },

  // *** Multistate Cluster Attributes ***
  {
    ZCL_CLUSTER_ID_GEN_MULTISTATE_INPUT_BASIC,
    { // Attribute record
      ATTRID_IOV_BASIC_PRESENT_VALUE,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_state
    }
  },

  // *** Metering Cluster Attributes ***
  {
    ZCL_CLUSTER_ID_SE_METERING,
    { // Attribute record
      ATTRID_CURRENT_SUM_DELIVERED,
      ZCL_DATATYPE_UINT48,
      ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
      (void *)&zclOpenEvse_energySum
    }
  },
  {
    ZCL_CLUSTER_ID_SE_METERING,
    { // Attribute record
      ATTRID_CURRENT_DEMAND_DELIVERED,
      ZCL_DATATYPE_UINT24,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_energyDemand
    }
  },
  {
    ZCL_CLUSTER_ID_SE_METERING,
    { // Attribute record
      ATTRID_CURRENT_DEMAND_LIMIT,
      ZCL_DATATYPE_UINT24,
      ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
      (void *)&zclOpenEvse_energyLimit
    }
  },

  // *** Electrical Measurement Cluster Attributes ***
  {
    ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT,
    { // Attribute record
      ATTRID_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE,
      ZCL_DATATYPE_BITMAP32,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_elecMeasType
    }
  },
  {
    ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT,
    { // Attribute record
      ATTRID_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_voltsScaled
    }
  },
  {
    ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT,
    { // Attribute record
      ATTRID_ELECTRICAL_MEASUREMENT_RMS_CURRENT,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_ampsScaled
    }
  },
  {
    ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT,
    { // Attribute record
      ATTRID_ELECTRICAL_MEASUREMENT_ACTIVE_POWER,
      ZCL_DATATYPE_INT16,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_wattsScaled
    }
  },
  {
    ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT,
    { // Attribute record
      ATTRID_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_MULTIPLIER,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_elecMeasMultiplier
    }
  },
  {
    ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT,
    { // Attribute record
      ATTRID_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_DIVISOR,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_elecMeasDivisor
    }
  },
  {
    ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT,
    { // Attribute record
      ATTRID_ELECTRICAL_MEASUREMENT_AC_CURRENT_MULTIPLIER,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_elecMeasMultiplier
    }
  },
  {
    ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT,
    { // Attribute record
      ATTRID_ELECTRICAL_MEASUREMENT_AC_CURRENT_DIVISOR,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_elecMeasDivisor
    }
  },
  {
    ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT,
    { // Attribute record
      ATTRID_ELECTRICAL_MEASUREMENT_AC_POWER_MULTIPLIER,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_elecMeasWattsMultiplier
    }
  },
  {
    ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT,
    { // Attribute record
      ATTRID_ELECTRICAL_MEASUREMENT_AC_POWER_DIVISOR,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_elecMeasWattsDivisor
    }
  }
};

uint8 CONST zclOpenEvse_NumAttributes = ( sizeof(zclOpenEvse_Attrs) / sizeof(zclOpenEvse_Attrs[0]) );

/*********************************************************************
 * SIMPLE DESCRIPTOR
 */
// This is the Cluster ID List and should be filled with Application
// specific cluster IDs.
const cId_t zclOpenEvse_InClusterList[] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_DEVICE_TEMP_CONFIG,
  ZCL_CLUSTER_ID_GEN_ON_OFF,
  ZCL_CLUSTER_ID_GEN_MULTISTATE_INPUT_BASIC,
  ZCL_CLUSTER_ID_SE_METERING,
  ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT
};
#define zclOpenEvse_MAX_INCLUSTERS   6

const cId_t zclOpenEvse_OutClusterList[] =
{
  ZCL_CLUSTER_ID_GEN_BASIC
};
#define zclOpenEvse_MAX_OUTCLUSTERS  (sizeof(zclOpenEvse_OutClusterList) / sizeof(zclOpenEvse_OutClusterList[0]))

SimpleDescriptionFormat_t zclOpenEvse_SimpleDesc =
{
  OPENEVSE_ENDPOINT,                  //  int Endpoint;
  ZCL_HA_PROFILE_ID,                  //  uint16 AppProfId;
  ZCL_HA_DEVICEID_SMART_PLUG,         //  uint16 AppDeviceId;
  OPENEVSE_DEVICE_VERSION,            //  int   AppDevVer:4;
  OPENEVSE_FLAGS,                     //  int   AppFlags:4;
  zclOpenEvse_MAX_INCLUSTERS,         //  byte  AppNumInClusters;
  (cId_t *)zclOpenEvse_InClusterList, //  byte *pAppInClusterList;
  zclOpenEvse_MAX_OUTCLUSTERS,        //  byte  AppNumInClusters;
  (cId_t *)zclOpenEvse_OutClusterList //  byte *pAppInClusterList;
};


/*********************************************************************
 * ATTRIBUTE DEFINITIONS - Uses REAL cluster IDs
 */
CONST zclAttrRec_t zclOpenEvse_BlAttrs[] =
{
  // *** General Basic Cluster Attributes ***
  {
    ZCL_CLUSTER_ID_GEN_BASIC,             // Cluster IDs - defined in the foundation (ie. zcl.h)
    {  // Attribute record
      ATTRID_BASIC_HW_VERSION,            // Attribute ID - Found in Cluster Library header (ie. zcl_general.h)
      ZCL_DATATYPE_UINT8,                 // Data Type - found in zcl.h
      ACCESS_CONTROL_READ,                // Variable access control - found in zcl.h
      (void *)&zclOpenEvse_HWRevision     // Pointer to attribute variable
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_ZCL_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_ZCLVersion
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_MANUFACTURER_NAME,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclOpenEvse_ManufacturerName
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_MODEL_ID,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclOpenEvse_ModelId
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_DATE_CODE,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclOpenEvse_DateCode
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_POWER_SOURCE,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_PowerSource
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_LOCATION_DESC,
      ZCL_DATATYPE_CHAR_STR,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)zclOpenEvse_LocationDescription
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_PHYSICAL_ENV,
      ZCL_DATATYPE_UINT8,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclOpenEvse_PhysicalEnvironment
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_DEVICE_ENABLED,
      ZCL_DATATYPE_BOOLEAN,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclOpenEvse_DeviceEnable
    }
  },

  // *** On/Off Cluster Attributes ***
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    { // Attribute record
      ATTRID_ON_OFF,
      ZCL_DATATYPE_BOOLEAN,
      ACCESS_CONTROL_READ,
      (void *)&zclOpenEvse_backlight
    }
  },
};

uint8 CONST zclOpenEvse_BlNumAttributes = ( sizeof(zclOpenEvse_BlAttrs) / sizeof(zclOpenEvse_BlAttrs[0]) );

/*********************************************************************
 * SIMPLE DESCRIPTOR
 */
// This is the Cluster ID List and should be filled with Application
// specific cluster IDs.
const cId_t zclOpenEvse_BlInClusterList[] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_ON_OFF,
};
#define zclOpenEvse_BL_MAX_INCLUSTERS   2

const cId_t zclOpenEvse_BlOutClusterList[] =
{
  ZCL_CLUSTER_ID_GEN_BASIC
};
#define zclOpenEvse_BL_MAX_OUTCLUSTERS  (sizeof(zclOpenEvse_BlOutClusterList) / sizeof(zclOpenEvse_BlOutClusterList[0]))

SimpleDescriptionFormat_t zclOpenEvse_BlSimpleDesc =
{
  OPENEVSE_ENDPOINT+1,                //  int Endpoint;
  ZCL_HA_PROFILE_ID,                  //  uint16 AppProfId;
  ZCL_HA_DEVICEID_ON_OFF_OUTPUT,      //  uint16 AppDeviceId;
  OPENEVSE_DEVICE_VERSION,            //  int   AppDevVer:4;
  OPENEVSE_FLAGS,                     //  int   AppFlags:4;
  zclOpenEvse_BL_MAX_INCLUSTERS,      //  byte  AppNumInClusters;
  (cId_t *)zclOpenEvse_BlInClusterList, //  byte *pAppInClusterList;
  zclOpenEvse_BL_MAX_OUTCLUSTERS,       //  byte  AppNumInClusters;
  (cId_t *)zclOpenEvse_BlOutClusterList //  byte *pAppInClusterList;
};


/*********************************************************************
 * GLOBAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/****************************************************************************
****************************************************************************/


