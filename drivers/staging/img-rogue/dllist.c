/*************************************************************************/ /*!
@File
@Title          Services implementation of double linked lists
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements a double linked list
@License        Strictly Confidential.
*/ /**************************************************************************/

#include "img_types.h"
#include "dllist.h"

#if defined(RGX_FIRMWARE)
#include "rgxfw_cr_defs.h"
#include "rgxfw_ctl.h"
#endif

/* Walk through all the nodes on the list until the end or a callback returns FALSE */
#if defined(RGX_FIRMWARE)
RGXFW_COREMEM_CODE
#endif
IMG_VOID dllist_foreach_node(PDLLIST_NODE psListHead,
							  PFN_NODE_CALLBACK pfnCallBack,
							  IMG_PVOID pvCallbackData)
{
	PDLLIST_NODE psWalker = psListHead->psNextNode;
	PDLLIST_NODE psNextWalker;

	while (psWalker != psListHead)
	{
		/*
			The callback function could remove itself from the list
			so to avoid NULL pointer deference save the next node pointer
			before calling the callback
		*/
		psNextWalker = psWalker->psNextNode;
		if (pfnCallBack(psWalker, pvCallbackData))
		{
			psWalker = psNextWalker;
		}
		else
		{
			break;
		}
	}
}

