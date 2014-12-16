/*************************************************************************/ /*!
@File
@Title          Common bridge header for timerquery
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures that are used by both
                the client and sever side of the bridge for timerquery
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef COMMON_TIMERQUERY_BRIDGE_H
#define COMMON_TIMERQUERY_BRIDGE_H

#include "rgx_bridge.h"


#include "pvr_bridge_io.h"

#define PVRSRV_BRIDGE_TIMERQUERY_CMD_FIRST			(PVRSRV_BRIDGE_TIMERQUERY_START)
#define PVRSRV_BRIDGE_TIMERQUERY_RGXBEGINTIMERQUERY			PVRSRV_IOWR(PVRSRV_BRIDGE_TIMERQUERY_CMD_FIRST+0)
#define PVRSRV_BRIDGE_TIMERQUERY_RGXENDTIMERQUERY			PVRSRV_IOWR(PVRSRV_BRIDGE_TIMERQUERY_CMD_FIRST+1)
#define PVRSRV_BRIDGE_TIMERQUERY_RGXQUERYTIMER			PVRSRV_IOWR(PVRSRV_BRIDGE_TIMERQUERY_CMD_FIRST+2)
#define PVRSRV_BRIDGE_TIMERQUERY_RGXCURRENTTIME			PVRSRV_IOWR(PVRSRV_BRIDGE_TIMERQUERY_CMD_FIRST+3)
#define PVRSRV_BRIDGE_TIMERQUERY_CMD_LAST			(PVRSRV_BRIDGE_TIMERQUERY_CMD_FIRST+3)


/*******************************************
            RGXBeginTimerQuery          
 *******************************************/

/* Bridge in structure for RGXBeginTimerQuery */
typedef struct PVRSRV_BRIDGE_IN_RGXBEGINTIMERQUERY_TAG
{
	IMG_HANDLE hDevNode;
	IMG_UINT32 ui32QueryId;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXBEGINTIMERQUERY;


/* Bridge out structure for RGXBeginTimerQuery */
typedef struct PVRSRV_BRIDGE_OUT_RGXBEGINTIMERQUERY_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXBEGINTIMERQUERY;

/*******************************************
            RGXEndTimerQuery          
 *******************************************/

/* Bridge in structure for RGXEndTimerQuery */
typedef struct PVRSRV_BRIDGE_IN_RGXENDTIMERQUERY_TAG
{
	IMG_HANDLE hDevNode;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXENDTIMERQUERY;


/* Bridge out structure for RGXEndTimerQuery */
typedef struct PVRSRV_BRIDGE_OUT_RGXENDTIMERQUERY_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXENDTIMERQUERY;

/*******************************************
            RGXQueryTimer          
 *******************************************/

/* Bridge in structure for RGXQueryTimer */
typedef struct PVRSRV_BRIDGE_IN_RGXQUERYTIMER_TAG
{
	IMG_HANDLE hDevNode;
	IMG_UINT32 ui32QueryId;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXQUERYTIMER;


/* Bridge out structure for RGXQueryTimer */
typedef struct PVRSRV_BRIDGE_OUT_RGXQUERYTIMER_TAG
{
	IMG_UINT64 ui64StartTime;
	IMG_UINT64 ui64EndTime;
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXQUERYTIMER;

/*******************************************
            RGXCurrentTime          
 *******************************************/

/* Bridge in structure for RGXCurrentTime */
typedef struct PVRSRV_BRIDGE_IN_RGXCURRENTTIME_TAG
{
	IMG_HANDLE hDevNode;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXCURRENTTIME;


/* Bridge out structure for RGXCurrentTime */
typedef struct PVRSRV_BRIDGE_OUT_RGXCURRENTTIME_TAG
{
	IMG_UINT64 ui64Time;
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXCURRENTTIME;

#endif /* COMMON_TIMERQUERY_BRIDGE_H */
