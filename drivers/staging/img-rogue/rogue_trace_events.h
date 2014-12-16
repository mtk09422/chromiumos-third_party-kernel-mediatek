/*************************************************************************/ /*!
@File
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/
#undef TRACE_SYSTEM
#define TRACE_SYSTEM rogue

#if !defined(_ROGUE_TRACE_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _ROGUE_TRACE_EVENTS_H

#include <linux/tracepoint.h>
#include <linux/time.h>

TRACE_EVENT(rogue_fence_update,

	TP_PROTO(const char *comm, const char *cmd, const char *dm, u32 fw_ctx, u32 offset,
		u32 sync_fwaddr, u32 sync_value),

	TP_ARGS(comm, cmd, dm, fw_ctx, offset, sync_fwaddr, sync_value),

	TP_STRUCT__entry(
		__string(       comm,           comm            )
		__string(       cmd,            cmd             )
		__string(       dm,             dm              )
		__field(        u32,            fw_ctx          )
		__field(        u32,            offset          )
		__field(        u32,            sync_fwaddr     )
		__field(        u32,            sync_value      )
	),

	TP_fast_assign(
		__assign_str(comm, comm);
		__assign_str(cmd, cmd);
		__assign_str(dm, dm);
		__entry->fw_ctx = fw_ctx;
		__entry->offset = offset;
		__entry->sync_fwaddr = sync_fwaddr;
		__entry->sync_value = sync_value;
	),

	TP_printk("comm=%s cmd=%s dm=%s fw_ctx=%lx offset=%lu sync_fwaddr=%lx sync_value=%lx",
		__get_str(comm),
		__get_str(cmd),
		__get_str(dm),
		(unsigned long)__entry->fw_ctx,
		(unsigned long)__entry->offset,
		(unsigned long)__entry->sync_fwaddr,
		(unsigned long)__entry->sync_value)
);

TRACE_EVENT(rogue_fence_check,

	TP_PROTO(const char *comm, const char *cmd, const char *dm, u32 fw_ctx, u32 offset,
		u32 sync_fwaddr, u32 sync_value),

	TP_ARGS(comm, cmd, dm, fw_ctx, offset, sync_fwaddr, sync_value),

	TP_STRUCT__entry(
		__string(       comm,           comm            )
		__string(       cmd,            cmd             )
		__string(       dm,             dm              )
		__field(        u32,            fw_ctx          )
		__field(        u32,            offset          )
		__field(        u32,            sync_fwaddr     )
		__field(        u32,            sync_value      )
	),

	TP_fast_assign(
		__assign_str(comm, comm);
		__assign_str(cmd, cmd);
		__assign_str(dm, dm);
		__entry->fw_ctx = fw_ctx;
		__entry->offset = offset;
		__entry->sync_fwaddr = sync_fwaddr;
		__entry->sync_value = sync_value;
	),

	TP_printk("comm=%s cmd=%s dm=%s fw_ctx=%lx offset=%lu sync_fwaddr=%lx sync_value=%lx",
		__get_str(comm),
		__get_str(cmd),
		__get_str(dm),
		(unsigned long)__entry->fw_ctx,
		(unsigned long)__entry->offset,
		(unsigned long)__entry->sync_fwaddr,
		(unsigned long)__entry->sync_value)
);

TRACE_EVENT(rogue_create_fw_context,

	TP_PROTO(const char *comm, const char *dm, u32 fw_ctx),

	TP_ARGS(comm, dm, fw_ctx),

	TP_STRUCT__entry(
		__string(       comm,           comm            )
		__string(       dm,             dm              )
		__field(        u32,            fw_ctx          )
	),

	TP_fast_assign(
		__assign_str(comm, comm);
		__assign_str(dm, dm);
		__entry->fw_ctx = fw_ctx;
	),

	TP_printk("comm=%s dm=%s fw_ctx=%lx",
		__get_str(comm),
		__get_str(dm),
		(unsigned long)__entry->fw_ctx)
);

#endif /* _ROGUE_TRACE_EVENTS_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .

/* This is needed because the name of this file doesn't match TRACE_SYSTEM. */
#define TRACE_INCLUDE_FILE rogue_trace_events

/* This part must be outside protection */
#include <trace/define_trace.h>
