/*************************************************************************/ /*!
@Title          Linux trace events and event helper functions
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(TRACE_EVENTS_H)
#define TRACE_EVENTS_H

#include "rogue_trace_events.h"

/* We need to make these functions do nothing if CONFIG_EVENT_TRACING isn't
 * enabled, just like the actual trace event functions that the kernel
 * defines for us.
 */
#ifdef CONFIG_EVENT_TRACING
void trace_rogue_fence_updates(const char *cmd, const char *dm,
							   IMG_UINT32 ui32FWContext,
							   IMG_UINT32 ui32Offset,
							   IMG_UINT uCount,
							   PRGXFWIF_UFO_ADDR *pauiAddresses,
							   IMG_UINT32 *paui32Values);

void trace_rogue_fence_checks(const char *cmd, const char *dm,
							  IMG_UINT32 ui32FWContext,
							  IMG_UINT32 ui32Offset,
							  IMG_UINT uCount,
							  PRGXFWIF_UFO_ADDR *pauiAddresses,
							  IMG_UINT32 *paui32Values);
#else  /* CONFIG_TRACE_EVENTS */
static inline
void trace_rogue_fence_updates(const char *cmd, const char *dm,
							   IMG_UINT32 ui32FWContext,
							   IMG_UINT32 ui32Offset,
							   IMG_UINT uCount,
							   PRGXFWIF_UFO_ADDR *pauiAddresses,
							   IMG_UINT32 *paui32Values)
{
}

static inline
void trace_rogue_fence_checks(const char *cmd, const char *dm,
							  IMG_UINT32 ui32FWContext,
							  IMG_UINT32 ui32Offset,
							  IMG_UINT uCount,
							  PRGXFWIF_UFO_ADDR *pauiAddresses,
							  IMG_UINT32 *paui32Values)
{
}
#endif /* CONFIG_TRACE_EVENTS */

#endif /* TRACE_EVENTS_H */
