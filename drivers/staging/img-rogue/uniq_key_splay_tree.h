/*************************************************************************/ /*!
@File
@Title          Splay trees interface
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Provides debug functionality
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef UNIQ_KEY_SPLAY_TREE_H_
#define UNIQ_KEY_SPLAY_TREE_H_

#include "img_types.h"

#if defined(__GNUC__) && defined(__x86_64__)
  /* note, the 64 bits requirements should not be necessary.
     Unfortunately, linking against the __ctzdi function (in 32bits) failed.  */

  #define HAS_BUILTIN_CTZLL
#endif

#if defined(HAS_BUILTIN_CTZLL)
  /* if the compiler can provide this builtin, then map the is_bucket_n_free?
     into an int. This way, the driver can find the first non empty without loop */

  typedef IMG_UINT64 IMG_ELTS_MAPPINGS;
#endif


/* head of list of free boundary tags for indexed by pvr_log2 of the
   boundary tag size */
#define FREE_TABLE_LIMIT 40

struct _BT_;

typedef struct img_splay_tree 
{
	/* left child/subtree */
    struct img_splay_tree * psLeft;

	/* right child/subtree */
    struct img_splay_tree * psRight;

    /* Flags to match on this span, used as the key. */
    IMG_UINT32 ui32Flags;

#if defined(HAS_BUILTIN_CTZLL)
	/* each bit of this int is a boolean telling if the corresponding
	   bucket is empty or not */
    IMG_ELTS_MAPPINGS bHasEltsMapping;
#endif
	
	struct _BT_ * buckets[FREE_TABLE_LIMIT];
} IMG_SPLAY_TREE, *IMG_PSPLAY_TREE;

IMG_PSPLAY_TREE PVRSRVSplay (IMG_UINT32 ui32Flags, IMG_PSPLAY_TREE psTree);
IMG_PSPLAY_TREE PVRSRVInsert(IMG_UINT32 ui32Flags, IMG_PSPLAY_TREE psTree);
IMG_PSPLAY_TREE PVRSRVDelete(IMG_UINT32 ui32Flags, IMG_PSPLAY_TREE psTree);


#endif /* !UNIQ_KEY_SPLAY_TREE_H_ */
