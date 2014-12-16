/*************************************************************************/ /*!
@File
@Title          RGX transfer queue shared
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Shared definitions between client and server
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef __RGX_TQ_SHARED_H__
#define __RGX_TQ_SHARED_H__

#define TQ_MAX_PREPARES_PER_SUBMIT		16

#define TQ_PREP_FLAGS_COMMAND_3D		0x0
#define TQ_PREP_FLAGS_COMMAND_2D		0x1
#define TQ_PREP_FLAGS_COMMAND_MASK		(0xf)
#define TQ_PREP_FLAGS_COMMAND_SHIFT		0
#define TQ_PREP_FLAGS_PDUMPCONTINUOUS	(1 << 4)
#define TQ_PREP_FLAGS_START				(1 << 5)
#define TQ_PREP_FLAGS_END				(1 << 6)

#define TQ_PREP_FLAGS_COMMAND_SET(m) \
	((TQ_PREP_FLAGS_COMMAND_##m << TQ_PREP_FLAGS_COMMAND_SHIFT) & TQ_PREP_FLAGS_COMMAND_MASK)

#define TQ_PREP_FLAGS_COMMAND_IS(m,n) \
	(((m & TQ_PREP_FLAGS_COMMAND_MASK) >> TQ_PREP_FLAGS_COMMAND_SHIFT)  == TQ_PREP_FLAGS_COMMAND_##n)

#endif /* __RGX_TQ_SHARED_H__ */
