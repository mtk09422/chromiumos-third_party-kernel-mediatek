/*************************************************************************/ /*!
@Title          C99-compatible types and definitions for Linux kernel code
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <linux/kernel.h>

/* Limits of specified-width integer types */

/* S8_MIN, etc were added in kernel version 3.14. The other versions are for
 * earlier kernels. They can be removed once older kernels don't need to be
 * supported.
 */
#ifdef S8_MIN
	#define INT8_MIN	S8_MIN
#else
	#define INT8_MIN	(-128)
#endif

#ifdef S8_MAX
	#define INT8_MAX	S8_MAX
#else
	#define INT8_MAX	127
#endif

#ifdef U8_MAX
	#define UINT8_MAX	U8_MAX
#else
	#define UINT8_MAX	0xFF
#endif

#ifdef S16_MIN
	#define INT16_MIN	S16_MIN
#else
	#define INT16_MIN	(-32768)
#endif

#ifdef S16_MAX
	#define INT16_MAX	S16_MAX
#else
	#define INT16_MAX	32767
#endif

#ifdef U16_MAX
	#define UINT16_MAX	U16_MAX
#else
	#define UINT16_MAX	0xFFFF
#endif

#ifdef S32_MIN
	#define INT32_MIN	S32_MIN
#else
	#define INT32_MIN	(-2147483647 - 1)
#endif

#ifdef S32_MAX
	#define INT32_MAX	S32_MAX
#else
	#define INT32_MAX	2147483647
#endif

#ifdef U32_MAX
	#define UINT32_MAX	U32_MAX
#else
	#define UINT32_MAX	0xFFFFFFFF
#endif

#ifdef S64_MIN
	#define INT64_MIN	S64_MIN
#else
	#define INT64_MIN	(-9223372036854775807LL)
#endif

#ifdef S64_MAX
	#define INT64_MAX	S64_MAX
#else
	#define INT64_MAX	9223372036854775807LL
#endif

#ifdef U64_MAX
	#define UINT64_MAX	U64_MAX
#else
	#define UINT64_MAX	0xFFFFFFFFFFFFFFFFULL
#endif

/* Macros for integer constants */
#define INT8_C			S8_C
#define UINT8_C			U8_C
#define INT16_C			S16_C
#define UINT16_C		U16_C
#define INT32_C			S32_C
#define UINT32_C		U32_C
#define INT64_C			S64_C
#define UINT64_C		U64_C

/* Format conversion of integer types <inttypes.h> */
/* Only define PRIX64 for the moment, as this is the only format macro that
 * img_types.h needs.
 */
#define PRIX64		"llX"
