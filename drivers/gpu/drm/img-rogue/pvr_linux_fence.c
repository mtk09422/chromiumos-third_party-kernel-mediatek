/*************************************************************************/ /*!
@File
@Title          Linux fence interface
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Linux module setup
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0))
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/fence.h>
#include <linux/list.h>

#include "img_types.h"
#include "pvrsrv_error.h"
#include "pvrsrv.h"
#include "sync_server_internal.h"
#include "sync_server.h"
#include "pvr_linux_fence.h"

struct fence_list
{
	struct fence **fences;
	unsigned count;
	unsigned cursor;
};

struct _LINUX_FENCE_CONTEXT_
{
	bool shared;
	struct mutex mutex;
	bool destroy_context;
	void *cmd_comp_notify;
	SERVER_SYNC_PRIMITIVE *attached_sync;
	IMG_UINT32 attached_fence_val;
	IMG_UINT32 attached_update_val;
	struct work_struct attached_fence_work;
	struct fence_list attached_fences;
	struct fence *attached_callback_fence;
	SERVER_SYNC_PRIMITIVE *created_sync;
	struct fence_cb attached_cb;
	struct fence *fence_to_signal;
	IMG_UINT32 created_fence_val;
	IMG_UINT32 created_update_val;
	struct work_struct created_fence_work;
};

struct pvr_fence
{
	struct fence fence;
	spinlock_t lock;
	atomic_t signal_countdown;
};

static struct workqueue_struct *workqueue;
static IMG_UINT32 syncID;
static unsigned fence_context;
static atomic_t fence_seqno = ATOMIC_INIT(0);
static atomic_t outstanding_fences = ATOMIC_INIT(0);

#if defined(PVR_DRM_DRIVER_NAME)
static const char *drvname = PVR_DRM_DRIVER_NAME;
#else
static const char *drvname = "pvr";
#endif
static const char *timeline_name = "PVR";

static unsigned next_seqno(void)
{
	return atomic_inc_return(&fence_seqno) - 1;
}

static const char *get_driver_name(struct fence *fence)
{
	return drvname;
}

static const char *get_timeline_name(struct fence *fence)
{
	return timeline_name;
}

static bool enable_signaling(struct fence *fence)
{
	return true;
}

static void release_fence(struct fence *fence)
{
	struct pvr_fence *pvr_fence = container_of(fence, struct pvr_fence, fence);

	kfree(pvr_fence);

	atomic_dec(&outstanding_fences);
}

static struct fence_ops fence_ops = 
{
	.get_driver_name = get_driver_name,
	.get_timeline_name = get_timeline_name,
	.enable_signaling = enable_signaling,
	.wait = fence_default_wait,
	.release = release_fence
};

static struct fence *create_linux_fence(void)
{
	struct pvr_fence *pvr_fence;
	unsigned seqno = next_seqno();

	pvr_fence = kmalloc(sizeof(*pvr_fence), GFP_KERNEL);
	if (!pvr_fence)
	{
		return NULL;
	}

	spin_lock_init(&pvr_fence->lock);
	atomic_set(&pvr_fence->signal_countdown, 1);

	fence_init(&pvr_fence->fence, &fence_ops, &pvr_fence->lock, fence_context, seqno);

	atomic_inc(&outstanding_fences);

	return &pvr_fence->fence;
}

static inline bool is_our_fence(struct fence *fence)
{
	return fence->ops == &fence_ops;
}

static inline void signal_and_put_our_fence(struct fence *fence)
{
	if (is_our_fence(fence))
	{
		struct pvr_fence *pvr_fence= container_of(fence, struct pvr_fence, fence);

		if (atomic_dec_and_test(&pvr_fence->signal_countdown))
		{
			fence_signal(fence);
		}

		fence_put(fence);
	}
}

static inline bool join_and_get_our_fence(struct fence *fence)
{
	if (is_our_fence(fence))
	{
		struct pvr_fence *pvr_fence = container_of(fence, struct pvr_fence, fence);
		if (atomic_inc_not_zero(&pvr_fence->signal_countdown))
		{
			fence_get(fence);
			return true;
		}
	}

	return false;
}

static void attached_fence_signaled(struct fence *fence, struct fence_cb *cb)
{
	struct _LINUX_FENCE_CONTEXT_ *lf = container_of(cb, struct _LINUX_FENCE_CONTEXT_, attached_cb);

	queue_work(workqueue, &lf->attached_fence_work);
}

static int install_attached_fence_callback(struct _LINUX_FENCE_CONTEXT_ *lf,
							struct fence *fence)
{
	int ret;

	if (fence && !is_our_fence(fence))
	{
		ret = fence_add_callback(fence,
				&lf->attached_cb,
				attached_fence_signaled);
		if (!ret)
		{
			lf->attached_callback_fence = fence;
		}
	}
	else
	{
		ret = 1;
	}

	return ret;
}

static int install_attached_fence_callback_and_get(struct _LINUX_FENCE_CONTEXT_ *lf,
							struct fence *fence)
{
	int ret; 

	ret = install_attached_fence_callback(lf, fence);
	if (!ret)
	{
		fence_get(fence);
	}

	return ret;
}

static int update_reservation_object_fences(struct _LINUX_FENCE_CONTEXT_ *lf,
						struct reservation_object *resv)
{
	struct reservation_object_list *flist;
	unsigned count, cursor;
	struct fence *new_fence = NULL;
	int ret;

	ret = ww_mutex_lock(&resv->lock, NULL);
	if (ret)
	{
		return ret;
	}

	memset(&lf->attached_fences, 0, sizeof(lf->attached_fences));

	flist = reservation_object_get_list(resv);
	count = flist ? flist->shared_count : 0;

	if (lf->shared)
	{
		/*
		 * There can't be more than one shared fence for a given
		 * fence context, so if one of our fences is already in
		 * the list, and hasn't been signalled, make use of it.
		 */
		for (cursor = 0; cursor < count; cursor++)
		{
			struct fence *fence = rcu_dereference_protected(flist->shared[cursor], reservation_object_held(resv));

			if (join_and_get_our_fence(fence))
			{
				new_fence = fence;
				break;
			}
		}

		if (!new_fence)
		{
			ret = reservation_object_reserve_shared(resv);
			if (ret)
			{
				goto unlock;
			}
		}
	}

	if (!new_fence)
	{
		new_fence = create_linux_fence();
		if (!new_fence)
		{
			ret = -ENOMEM;
			goto unlock;
		}
	}

	lf->fence_to_signal = new_fence;

	if (lf->shared || !count)
	{
		struct fence *fence = reservation_object_get_excl(resv);

		ret = install_attached_fence_callback_and_get(lf, fence) ? 1 : 0;

		if (lf->shared)
		{
			reservation_object_add_shared_fence(resv, new_fence);
			goto unlock;
		}
		else
		{
			goto add_excl_fence;
		}
	}

	for (cursor = 0; cursor < count; cursor++)
	{

		struct fence *fence = rcu_dereference_protected(flist->shared[cursor], reservation_object_held(resv));

		ret = install_attached_fence_callback_and_get(lf, fence);
		if (!ret)
		{
			break;
		}
	}

	if (cursor == count)
	{
		/* No blocking fences */
		ret = 1;
	}
	else if (++cursor == count)
	{
		/* Callback installed, and no non-signalled fences remaining */
		ret = 0;
	}
	else
	{
		/*
		 * Callback installed, and perhaps some non-signalled fences
		 * remaining.
		 */
		ret = 0;

		/* Skip to the first fence that isn't one of ours */
		for (/* cursor = cursor */; cursor < count; cursor++)
		{
			struct fence *fence = rcu_dereference_protected(flist->shared[cursor], reservation_object_held(resv));

			if (!is_our_fence(fence))
			{
				break;
			}
		}

		count -= cursor;
		if (count)
		{
			lf->attached_fences.fences = kmalloc(count * sizeof(struct fence *), GFP_KERNEL);
			if (lf->attached_fences.fences)
			{
				unsigned i;

				for (i = 0; i < count; i++)
				{
					struct fence *fence = rcu_dereference_protected(flist->shared[cursor + i], reservation_object_held(resv));

					fence_get(fence);
					lf->attached_fences.fences[i] = fence;
				}
				lf->attached_fences.count = count;
			}
			else
			{
				fence_remove_callback(lf->attached_callback_fence, &lf->attached_cb);
				fence_put(lf->attached_callback_fence);
				lf->attached_callback_fence = NULL;

				fence_put(new_fence);
				lf->fence_to_signal = NULL;

				ret = -ENOMEM;
				goto unlock;
			}
		}
	}

add_excl_fence:
	reservation_object_add_excl_fence(resv, new_fence);
unlock:
	ww_mutex_unlock(&resv->lock);

	return ret;
}

static int next_attached_fence(struct _LINUX_FENCE_CONTEXT_ *lf)
{
	unsigned count = lf->attached_fences.count;
	unsigned cursor = lf->attached_fences.cursor;
	int ret;

	if (lf->attached_callback_fence)
	{
		fence_put(lf->attached_callback_fence);
		lf->attached_callback_fence = NULL;
	}

	for (cursor = 0; cursor < count; cursor++)
	{

		struct fence *fence = lf->attached_fences.fences[cursor];

		ret = install_attached_fence_callback(lf, fence);
		if (ret)
		{
			fence_put(fence);
		}
		else
		{
			break;
		}
	}

	if (++cursor >= count)
	{
		if (count)
		{
			kfree(lf->attached_fences.fences);
			memset(&lf->attached_fences, 0, sizeof(lf->attached_fences));
		}
		ret = (cursor == count) ? 0 : 1;
	}
	else
	{
		lf->attached_fences.count = cursor;
		ret = 0;
	}
		
	return ret;
}

static void destroy_fence_context(LINUX_FENCE_CONTEXT *lf)
{
	if (lf->cmd_comp_notify)
	{
		PVRSRVUnregisterCmdCompleteNotify(lf->cmd_comp_notify);
	}

	mutex_destroy(&lf->mutex);

	kfree(lf);
}

static void do_attached_fence_work(struct work_struct *work)
{
	struct _LINUX_FENCE_CONTEXT_ *lf = container_of(work, struct _LINUX_FENCE_CONTEXT_, attached_fence_work);
	bool check_status = false;
	bool destroy_context = false;

	mutex_lock(&lf->mutex);
	if (lf->attached_sync && next_attached_fence(lf) &&
		ServerSyncFenceIsMet(lf->attached_sync, lf->attached_fence_val))
	{
		check_status = true;

		ServerSyncCompleteOp(lf->attached_sync, IMG_TRUE, lf->attached_update_val);

		lf->attached_sync = NULL;

		destroy_context = lf->destroy_context && !lf->created_sync;
	}
	mutex_unlock(&lf->mutex);

	if (check_status)
	{
		PVRSRVCheckStatus(lf);
	}

	if (destroy_context)
	{
		destroy_fence_context(lf);
	}
}

static void do_created_fence_work(struct work_struct *work)
{
	struct _LINUX_FENCE_CONTEXT_ *lf = container_of(work, struct _LINUX_FENCE_CONTEXT_, created_fence_work);
	SERVER_SYNC_PRIMITIVE *sync = NULL;
	bool destroy_context = false;

	mutex_lock(&lf->mutex);
	if (lf->created_sync &&
		ServerSyncFenceWasMet(lf->created_sync, lf->created_fence_val))
	{
		signal_and_put_our_fence(lf->fence_to_signal);
		lf->fence_to_signal = NULL;

		sync = lf->created_sync;
		lf->created_sync = NULL;

		destroy_context = lf->destroy_context && !lf->attached_sync;
	}
	mutex_unlock(&lf->mutex);

	if (sync)
	{
		ServerSyncCompletePassedFenceOp(sync);
	}

	if (destroy_context)
	{
		destroy_fence_context(lf);
	}
}

static void cmd_complete(void *data)
{
	struct _LINUX_FENCE_CONTEXT_ *lf = data;

	mutex_lock(&lf->mutex);

	if (lf->attached_sync &&
		!lf->attached_callback_fence &&
		ServerSyncFenceIsMet(lf->attached_sync, lf->attached_fence_val))
	{
		queue_work(workqueue, &lf->attached_fence_work);
	}

	if (lf->created_sync &&
		ServerSyncFenceWasMet(lf->created_sync, lf->created_fence_val))
	{
		queue_work(workqueue, &lf->created_fence_work);
	}

	mutex_unlock(&lf->mutex);
}

void pvr_linux_destroy_fence_context(LINUX_FENCE_CONTEXT *lf)
{
	bool destroy_context = false;

	mutex_lock(&lf->mutex);
	lf->destroy_context = true;
	if (!lf->created_sync)
	{
		if (lf->fence_to_signal)
		{
			signal_and_put_our_fence(lf->fence_to_signal);
			lf->fence_to_signal = NULL;
		}
		destroy_context = !lf->attached_sync;
	}
	mutex_unlock(&lf->mutex);

	if (destroy_context)
	{
		destroy_fence_context(lf);
	}
}

int pvr_linux_create_fence_context(LINUX_FENCE_CONTEXT **plf, bool shared)
{
	struct _LINUX_FENCE_CONTEXT_ *lf;
	PVRSRV_ERROR eError;

	lf = kzalloc(sizeof(*lf), GFP_KERNEL);
	if (!lf)
	{
		return -ENOMEM;
	}

	lf->shared = shared;

	mutex_init(&lf->mutex);
	INIT_WORK(&lf->attached_fence_work, do_attached_fence_work);
	INIT_WORK(&lf->created_fence_work, do_created_fence_work);

	eError = PVRSRVRegisterCmdCompleteNotify(&lf->cmd_comp_notify,
							cmd_complete,
							lf);
	if (eError != PVRSRV_OK)
	{
		goto error_destroy_mutex;
	}
						
	*plf = lf;

	return 0;

error_destroy_mutex:
	mutex_destroy(&lf->mutex);
	kfree(lf);
	return -ENOMEM;
}

int pvr_linux_fence_attach(LINUX_FENCE_CONTEXT *lf, SERVER_SYNC_PRIMITIVE *sync, struct reservation_object *resv)
{
	IMG_BOOL fence_required;
	PVRSRV_ERROR eError;
	int ret;

	if (!resv)
	{
		return 0;
	}

	mutex_lock(&lf->mutex);

	if (lf->attached_sync)
	{
		ret = (lf->attached_sync == sync) ? 0 : -EBUSY;
		goto error_unlock;
	}

	if (lf->fence_to_signal)
	{
		if (lf->created_sync)
		{
			ret = -EBUSY;
			goto error_unlock;
		}
		else
		{
			signal_and_put_our_fence(lf->fence_to_signal);
			lf->fence_to_signal = NULL;
		}
	}

	ret = update_reservation_object_fences(lf, resv);
	if (ret < 0)
	{
		goto error_unlock;
	}
	else if (!ret)
	{
		eError = PVRSRVServerSyncQueueSWOpKM(sync,
					&lf->attached_fence_val,
					&lf->attached_update_val,
					syncID,
					IMG_TRUE,
					&fence_required);
		BUG_ON(eError != PVRSRV_OK);

		lf->attached_sync = sync;
	}

	mutex_unlock(&lf->mutex);

	return 0;

error_unlock:
	mutex_unlock(&lf->mutex);
	return ret;
}

int pvr_linux_fence_create(LINUX_FENCE_CONTEXT *lf, SERVER_SYNC_PRIMITIVE *sync)
{
	IMG_BOOL fence_required;
	PVRSRV_ERROR eError;
	int ret;

	mutex_lock(&lf->mutex);

	if (!lf->fence_to_signal)
	{
		ret = -ENOENT;
		goto error_unlock;
	}

	if (lf->created_sync)
	{
		ret = -EBUSY;
		goto error_unlock;
	}

	eError = PVRSRVServerSyncQueueSWOpKM(sync,
					&lf->created_fence_val,
					&lf->created_update_val,
					syncID,
					IMG_FALSE,
					&fence_required);
	BUG_ON(eError != PVRSRV_OK);

	lf->created_sync = sync;

	if (ServerSyncFenceWasMet(sync, lf->created_fence_val))
	{
		queue_work(workqueue, &lf->created_fence_work);
	}

	mutex_unlock(&lf->mutex);

	return 0;

error_unlock:
	mutex_unlock(&lf->mutex);
	return ret;
}

void pvr_linux_fence_deinit(void)
{
	unsigned fences_remaining;

	if (workqueue)
	{
		destroy_workqueue(workqueue);
	}

	if (syncID != 0)
	{
		PVRSRVServerSyncRequesterUnregisterKM(syncID);
	}

	fences_remaining = atomic_read(&outstanding_fences);
	if (fences_remaining)
	{
		printk(KERN_WARNING "%s: %u fences leaked\n",
				__func__, fences_remaining);
	}
}

int pvr_linux_fence_init(void)
{
	int ret = -ENOMEM;
	PVRSRV_ERROR eError;

	eError = PVRSRVServerSyncRequesterRegisterKM(&syncID);
	if (eError != PVRSRV_OK)
	{
		goto error;
	}

	workqueue = create_workqueue("PVR Linux Fence");
	if (!workqueue)
	{
		goto error;
	}

	fence_context = fence_context_alloc(1);

	return 0;
error:
	pvr_linux_fence_deinit();
	return ret;
}
#else	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,17)) */
int pvr_linux_create_fence_context(LINUX_FENCE_CONTEXT **psContext)
{
	return 0;
}

void pvr_linux_destroy_fence_context(LINUX_FENCE_CONTEXT *psContext, bool shared)
{
}

int pvr_linux_fence_attach(LINUX_FENCE_CONTEXT *psContext, SERVER_SYNC_PRIMITIVE *psSync, struct reservation_object *resv)
{
	return 0;
}

int pvr_linux_fence_create(LINUX_FENCE_CONTEXT *psContext, SERVER_SYNC_PRIMITIVE *psSync)
{
	return 0;
}

void pvr_linux_fence_deinit(void)
{
}

int pvr_linux_fence_init(void)
{
	return 0;
}
#endif	/* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,17)) */
