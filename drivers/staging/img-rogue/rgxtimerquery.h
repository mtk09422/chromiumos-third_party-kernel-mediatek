/*************************************************************************/ /*!
@File
@Title          RGX Timer queries
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX Timer queries functionality
@License        Strictly Confidential.
*/ /**************************************************************************/

#if ! defined (_RGX_TIMERQUERIES_H_)
#define _RGX_TIMERQUERIES_H_

#include "pvrsrv_error.h"
#include "img_types.h"
#include "device.h"
#include "rgxdevice.h"

/**************************************************************************/ /*!
@Function       PVRSRVRGXBeginTimerQuery
@Description    Opens a new timer query.

@Input          ui32QueryId an identifier between [ 0 and RGX_MAX_TIMER_QUERIES - 1 ]
@Return         PVRSRV_OK on success.
*/ /***************************************************************************/
PVRSRV_ERROR
PVRSRVRGXBeginTimerQueryKM(PVRSRV_DEVICE_NODE * psDeviceNode,
                           IMG_UINT32         ui32QueryId);


/**************************************************************************/ /*!
@Function       PVRSRVRGXEndTimerQuery
@Description    Closes a timer query

                The lack of ui32QueryId argument expresses the fact that there can't
                be overlapping queries open.
@Return         PVRSRV_OK on success.
*/ /***************************************************************************/
PVRSRV_ERROR
PVRSRVRGXEndTimerQueryKM(PVRSRV_DEVICE_NODE * psDeviceNode);



/**************************************************************************/ /*!
@Function       PVRSRVRGXQueryTimer
@Description    Queries the state of the specified timer

@Input          ui32QueryId an identifier between [ 0 and RGX_MAX_TIMER_QUERIES - 1 ]
@Out            pui64StartTime
@Out            pui64EndTime
@Return         PVRSRV_OK                         on success.
                PVRSRV_ERROR_RESOURCE_UNAVAILABLE if the device is still busy with
                                                  operations from the queried period
                other error code                  otherwise
*/ /***************************************************************************/
PVRSRV_ERROR
PVRSRVRGXQueryTimerKM(PVRSRV_DEVICE_NODE * psDeviceNode,
                      IMG_UINT32         ui32QueryId,
                      IMG_UINT64         * pui64StartTime,
                      IMG_UINT64         * pui64EndTime);


/**************************************************************************/ /*!
@Function       PVRSRVRGXCurrentTime
@Description    Returns the current state of the timer used in timer queries
@Input          psDevData  Device data.
@Out            pui64Time
@Return         PVRSRV_OK on success.
*/ /***************************************************************************/
PVRSRV_ERROR
PVRSRVRGXCurrentTime(PVRSRV_DEVICE_NODE * psDeviceNode,
                     IMG_UINT64         * pui64Time);


/******************************************************************************
 NON BRIDGED/EXPORTED interface
******************************************************************************/

/* write the timestamp cmd from the helper*/
IMG_VOID
RGXWriteTimestampCommand(IMG_PBYTE            * ppui8CmdPtr,
                         RGXFWIF_CCB_CMD_TYPE eCmdType,
                         RGXFWIF_DEV_VIRTADDR pTimestamp);

/* get the relevant data from the Kick to the helper*/
IMG_VOID
RGX_GetTimestampCmdHelper(PVRSRV_RGXDEV_INFO   * psDevInfo,
                          RGXFWIF_DEV_VIRTADDR * ppPreTimestamp,
                          RGXFWIF_DEV_VIRTADDR * ppPostTimestamp,
                          PRGXFWIF_UFO_ADDR    * ppUpdate);

#endif /* _RGX_TIMERQUERIES_H_ */

/******************************************************************************
 End of file (rgxtimerquery.h)
******************************************************************************/
