/*************************************************************************/ /*!
@File
@Title          Device Memory Management core
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Client side part of device memory management -- This
                file defines the exposed Services API to core memory management
                functions.
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

#ifndef PVRSRV_DEVMEM_H
#define PVRSRV_DEVMEM_H

#if defined __cplusplus
extern "C" {
#endif

#include "img_types.h"
#include "devicemem_typedefs.h"
#include "pdumpdefs.h"
#include "pvrsrv_error.h"
#include "pvrsrv_memallocflags.h"
#include "services.h"	/* For PVRSRV_DEV_DATA */
#include "sync_external.h"

struct _PVRSRV_DEVMEMCTX_;

/*
  Device memory contexts, heaps and memory descriptors are passed
  through to underlying memory APIs directly, but are to be regarded
  as an opaque handle externally.
*/
typedef struct _PVRSRV_DEVMEMCTX_ *PVRSRV_DEVMEMCTX;       /*!< Device-Mem Client-Side Interface: Typedef for Context Ptr */
typedef DEVMEM_HEAP *PVRSRV_HEAP;               /*!< Device-Mem Client-Side Interface: Typedef for Heap Ptr */
typedef DEVMEM_MEMDESC *PVRSRV_MEMDESC;         /*!< Device-Mem Client-Side Interface: Typedef for Memory Descriptor Ptr */
typedef DEVMEM_EXPORTCOOKIE PVRSRV_DEVMEM_EXPORTCOOKIE;     /*!< Device-Mem Client-Side Interface: Typedef for Export Cookie */
typedef DEVMEM_FLAGS_T PVRSRV_MEMMAP_FLAGS_T;               /*!< Device-Mem Client-Side Interface: Typedef for Memory-Mapping Flags Enum */

/* N.B.  Flags are now defined in pvrsrv_memallocflags.h as they need
         to be omnipresent. */

/*
 *
 *  API functions
 *
 */

/**************************************************************************/ /*!
@Function       PVRSRVCreateDeviceMemContext
@Description    Creates a device memory context.  There is a one-to-one
                correspondence between this context data structure and the top
                level MMU page table (known as the Page Catalogue, in the case of a
                3-tier MMU).  It is intended that a process with its own virtual
                space on the CPU will also have its own virtual space on the GPU.
                Thus there is loosely a one-to-one correspondence between process
                and device memory context, but this is not enforced at this API.
 
                Every process must create the device memory context before any
                memory allocations are made, and is responsible for freeing all
                such allocations before destroying the context
     
                This is a wrapper function above the "bare-metal" device memory
                context creation function which would create just a context and no
                heaps.  This function will also create the heaps, according to the
                heap config that the device specific initialization code has
                nominated for use by this API.
     
                The number of heaps thus created is returned to the caller, such
                that the caller can allocate an array and the call in to fetch
                details of each heap, or look up the heap with the "Find Heap" API
                described below.
     
                In order to derive the details of the MMU configuration for the
                device, and for retrieving the "bridge handle" for communication
                internally in services, is is necessary to pass in the
                PVRSRV_DEV_DATA object as populated with a prior call to
                PVRSRVAcquireDeviceData()
@Input          psDev           dev data
@Output         phCtxOut        On success, the returned DevMem Context. The
                                caller is responsible for providing storage
                                for this.
@Return         PVRSRV_ERROR:   PVRSRV_OK on success. Otherwise, a PVRSRV_
                                error code
*/ /***************************************************************************/
extern IMG_IMPORT PVRSRV_ERROR
PVRSRVCreateDeviceMemContext(PVRSRV_DEV_CONNECTION *psDevConnection,
                             PVRSRV_DEVMEMCTX *phCtxOut);

/**************************************************************************/ /*!
@Function       PVRSRVDestroyDeviceMemContext
@Description    Destroy cannot fail.  Well.  It shouldn't, assuming the caller
                has obeyed the protocol, i.e. has freed all his allocations 
                beforehand.
@Input          hCtx            Handle to a DevMem Context
@Return         None
*/ /***************************************************************************/
extern IMG_IMPORT void
PVRSRVDestroyDeviceMemContext(PVRSRV_DEVMEMCTX hCtx);

/**************************************************************************/ /*!
@Function       PVRSRVFindHeapByName
@Description    Returns the heap handle for the named heap which is assumed to
                exist in this context. PVRSRV_HEAP *phHeapOut,  

                N.B.  No need for acquire/release semantics here, as when using
                this wrapper layer, the heaps are automatically instantiated at
                context creation time and destroyed when the context is 
                destroyed.

                The caller is required to know the heap names already as these 
                will vary from device to device and from purpose to purpose.
@Input          hCtx            Handle to a DevMem Context
@Input          pszHeapName     Name of the heap to look for
@Output         phHeapOut       a handle to the heap, for use in future calls 
                                to OpenAllocation / AllocDeviceMemory / Map 
                                DeviceClassMemory, etc. (The PVRSRV_HEAP type
                                to be regarded by caller as an opaque, but 
                                strongly typed, handle)
@Return         PVRSRV_ERROR:   PVRSRV_OK on success. Otherwise, a PVRSRV_
                                error code
*/ /***************************************************************************/
extern IMG_IMPORT PVRSRV_ERROR
PVRSRVFindHeapByName(PVRSRV_DEVMEMCTX hCtx,
                     const IMG_CHAR *pszHeapName,
                     PVRSRV_HEAP *phHeapOut);

/**************************************************************************/ /*!
@Function       PVRSRVDevmemGetHeapBaseDevVAddr
@Description    returns the device virtual address of the base of the heap.
@Input          hHeap           Handle to a Heap
@Output         pDevVAddr       On success, the device virtual address of the
                                base of the heap.
@Return         PVRSRV_ERROR:   PVRSRV_OK on success. Otherwise, a PVRSRV_
                                error code
*/ /***************************************************************************/
IMG_IMPORT PVRSRV_ERROR
PVRSRVDevmemGetHeapBaseDevVAddr(PVRSRV_HEAP hHeap,
			        IMG_DEV_VIRTADDR *pDevVAddr);

/**************************************************************************/ /*!
@Function       PVRSRVAllocDeviceMem
@Description    Allocate memory from the specified heap, acquiring physical
                memory from OS as we go and mapping this into
                the GPU (mandatorily) and CPU (optionally)

                Size must be a positive integer multiple of alignment, or, to
                put it another way, the uiLog2Align LSBs should all be zero, but
                at least one other bit should not be.

                Caller to take charge of the PVRSRV_MEMDESC (the memory
                descriptor) which is to be regarded as an opaque handle.

                If the allocation is supposed to be used with PVRSRVDevmemUnpin()
                the size must be a page multiple.
                This is a general rule when suballocations are to
                be avoided.

@Input          hHeap               Handle to the heap from which memory will be
                                    allocated
@Input          uiSize              Amount of memory to be allocated.
@Input          uiLog2Align         LOG2 of the required alignment
@Input          uiMemAllocFlags     Allocation Flags
@Input          pszText     		Text to describe the allocation
@Output         phMemDescOut        On success, the resulting memory descriptor
@Return         PVRSRV_OK on success. Otherwise, a PVRSRV_ error code
*/ /***************************************************************************/
extern IMG_IMPORT PVRSRV_ERROR
PVRSRVAllocDeviceMem(PVRSRV_HEAP hHeap,
                     IMG_DEVMEM_SIZE_T uiSize,
                     IMG_DEVMEM_LOG2ALIGN_T uiLog2Align,
                     PVRSRV_MEMALLOCFLAGS_T uiMemAllocFlags,
                     IMG_PCHAR pszText,
                     PVRSRV_MEMDESC *phMemDescOut);

/**************************************************************************/ /*!
@Function       PVRSRVFreeDeviceMem
@Description    Free that allocated by PVRSRVAllocDeviceMem (Memory descriptor 
                will be destroyed)
@Input          hMemDesc            Handle to the descriptor of the memory to be
                                    freed
@Return         None
*/ /***************************************************************************/
extern IMG_IMPORT void
PVRSRVFreeDeviceMem(PVRSRV_MEMDESC hMemDesc);

/**************************************************************************/ /*!
@Function       PVRSRVAcquireCPUMapping
@Description    Causes the allocation referenced by this memory descriptor to be
                mapped into cpu virtual memory, if it wasn't already, and the
                CPU virtual address returned in the caller-provided location.

                The caller must call PVRSRVReleaseCPUMapping to advise when he
                has finished with the mapping.

                Does not accept unpinned allocations.
                Returns PVRSRV_ERROR_INVALID_MAP_REQUEST if an unpinned
                MemDesc is passed in.

@Input          hMemDesc            Handle to the memory descriptor for which a
                                    CPU mapping is required
@Output         ppvCpuVirtAddrOut   On success, the caller's ptr is set to the
                                    new CPU mapping
@Return         PVRSRV_ERROR:       PVRSRV_OK on success. Otherwise, a PVRSRV_
                                    error code
*/ /***************************************************************************/
extern IMG_IMPORT PVRSRV_ERROR
PVRSRVAcquireCPUMapping(PVRSRV_MEMDESC hMemDesc,
                        void **ppvCpuVirtAddrOut);

/**************************************************************************/ /*!
@Function       PVRSRVReleaseCPUMapping
@Description    Relinquishes the cpu mapping acquired with 
                PVRSRVAcquireCPUMapping()
@Input          hMemDesc            Handle of the memory descriptor
@Return         None
*/ /***************************************************************************/
extern IMG_IMPORT void
PVRSRVReleaseCPUMapping(PVRSRV_MEMDESC hMemDesc);


/**************************************************************************/ /*!
@Function       PVRSRVMapToDevice
@Description    Map allocation into the device MMU. This function must only be
                called once, any further calls will return
                PVRSRV_ERROR_DEVICEMEM_ALREADY_MAPPED

                The caller must call PVRSRVReleaseDeviceMapping when they
                are finished with the mapping.

                Does not accept unpinned allocations.
                Returns PVRSRV_ERROR_INVALID_MAP_REQUEST if an unpinned
                MemDesc is passed in.

@Input          hMemDesc            Handle of the memory descriptor
@Input          hHeap               Device heap to map the allocation into
@Output         psDevVirtAddrOut    Device virtual address
@Return         PVRSRV_ERROR:       PVRSRV_OK on success. Otherwise, a PVRSRV_
                                    error code
*/ /***************************************************************************/
extern IMG_IMPORT PVRSRV_ERROR
PVRSRVMapToDevice(PVRSRV_MEMDESC hMemDesc,
				  PVRSRV_HEAP hHeap,
				  IMG_DEV_VIRTADDR *psDevVirtAddrOut);

/**************************************************************************/ /*!
@Function       PVRSRVAcquireDeviceMapping
@Description    Acquire a reference on the device mapping the allocation.
                If the allocation wasn't mapped into the device then 
                and the device virtual address returned in the
                PVRSRV_ERROR_DEVICEMEM_NO_MAPPING will be returned as
                PVRSRVMapToDevice must be called first.

                The caller must call PVRSRVReleaseDeviceMapping when they
                are finished with the mapping.

                Does not accept unpinned allocations.
                Returns PVRSRV_ERROR_INVALID_MAP_REQUEST if an unpinned
                MemDesc is passed in.

@Input          hMemDesc            Handle to the memory descriptor for which a
                                    device mapping is required
@Output         psDevVirtAddrOut    On success, the caller's ptr is set to the
                                    new device mapping
@Return         PVRSRV_ERROR:       PVRSRV_OK on success. Otherwise, a PVRSRV_
                                    error code
*/ /***************************************************************************/
extern IMG_IMPORT PVRSRV_ERROR
PVRSRVAcquireDeviceMapping(PVRSRV_MEMDESC hMemDesc,
						   IMG_DEV_VIRTADDR *psDevVirtAddrOut);

/**************************************************************************/ /*!
@Function       PVRSRVReleaseDeviceMapping
@Description    Relinquishes the device mapping acquired with
                PVRSRVAcquireDeviceMapping or PVRSRVMapToDevice
@Input          hMemDesc            Handle of the memory descriptor
@Return         None
*/ /***************************************************************************/
extern IMG_IMPORT void
PVRSRVReleaseDeviceMapping(PVRSRV_MEMDESC hMemDesc);

/*************************************************************************/ /*!
@Function       PVRSRVDevmemLocalImport

@Description    Import a PMR that was created with this connection.
                The general usage of this function is as follows:
                1) Create a devmem allocation on server side.
                2) Pass back the PMR of that allocation to client side by
                   creating a handle of type PMR_LOCAL_EXPORT_HANDLE.
                3) Pass the PMR_LOCAL_EXPORT_HANDLE to
                   PVRSRVMakeLocalImportHandle()to create a new handle type
                   (DEVMEM_MEM_IMPORT) that can be used with this function.

@Input          hExtHandle              External memory handle

@Input          uiFlags                 Import flags

@Output         phMemDescPtr            Created MemDesc

@Output         puiSizePtr              Size of the created MemDesc

@Return         PVRSRV_OK is succesful
*/
/*****************************************************************************/
PVRSRV_ERROR PVRSRVDevmemLocalImport(const PVRSRV_DEV_CONNECTION *psDevConnection,
									 IMG_HANDLE hExtHandle,
									 PVRSRV_MEMMAP_FLAGS_T uiFlags,
									 PVRSRV_MEMDESC *phMemDescPtr,
									 IMG_DEVMEM_SIZE_T *puiSizePtr);

/*************************************************************************/ /*!
@Function       PVRSRVDevmemGetImportUID

@Description    Get the UID of the import that backs this MemDesc

@Input          hMemDesc                MemDesc

@Return         UID of import
*/
/*****************************************************************************/
PVRSRV_ERROR PVRSRVDevmemGetImportUID(PVRSRV_MEMDESC hMemDesc,
									  IMG_UINT64 *pui64UID);

/**************************************************************************/ /*!
@Function       PVRSRVAllocExportableDevMem
@Description    Allocate memory without mapping into device memory context.  This
                memory is exported and ready to be mapped into the device memory
                context of other processes, or to CPU only with 
                PVRSRVMapMemoryToCPUOnly(). The caller agrees to later call 
                PVRSRVFreeUnmappedExportedMemory(). The caller must give the page
                size of the heap into which this memory may be subsequently 
                mapped, or the largest of such page sizes if it may be mapped 
                into multiple places.  This information is to be communicated in
                the Log2Align field.

                Size must be a positive integer multiple of the page size
@Input          uiLog2Align         Log2 of the alignment required
@Input          uiSize              the amount of memory to be allocated
@Input          uiFlags             Allocation flags
@Input          pszText     		Text to describe the allocation
@Output         hMemDesc
@Return         PVRSRV_OK on success. Otherwise, a PVRSRV_ error code
*/ /***************************************************************************/
PVRSRV_ERROR
PVRSRVAllocExportableDevMem(const PVRSRV_DEV_CONNECTION *psDevConnection,
							IMG_DEVMEM_SIZE_T uiSize,
							IMG_DEVMEM_LOG2ALIGN_T uiLog2Align,
							PVRSRV_MEMALLOCFLAGS_T uiFlags,
							IMG_PCHAR pszText,
							PVRSRV_MEMDESC *hMemDesc);

/**************************************************************************/ /*!
@Function       PVRSRVChangeSparseDevMem
@Description	This function alters the underlying memory layout of the given
				allocation by allocating/removing pages as requested
				This function also re-writes the GPU & CPU Maps accordingly
				The specific actions can be controlled by corresponding flags
@Input			psMemDesc 			The memory layout that needs to be modified
@Input			ui32AllocPageCount	New page allocation count
@Input			pai32AllocIndices   New page allocation indices (page granularity)
@Input			ui32FreePageCount   Number of pages that need to be freed
@Input			pai32FreeIndices	Indices of the pages that need to be freed
@Input			uiFlags				Flags that control the behaviour of the call
@Output			pui32Status			Status out for minor tolerable errors
@Return         PVRSRV_OK on success. Otherwise, a PVRSRV_ error code
*/ /***************************************************************************/
PVRSRV_ERROR
PVRSRVChangeSparseDevMem(PVRSRV_MEMDESC psMemDesc,
					IMG_UINT32 ui32AllocPageCount,
					IMG_UINT32 *pai32AllocIndices,
					IMG_UINT32 ui32FreePageCount,
					IMG_UINT32 *pai32FreeIndices,
					SPARSE_MEM_RESIZE_FLAGS uiFlags,
					IMG_UINT32 *pui32Status);

/**************************************************************************/ /*!
@Function       PVRSRVAllocSparseDevMem2
@Description    Allocate sparse memory without mapping into device memory context.
				Sparse memory is used where you have an allocation that has a
				logical size (i.e. the amount of VM space it will need when
				mapping it into a device) that is larger then the amount of
				physical memory that allocation will use. An example of this
				is a NPOT texture where the twiddling algorithm requires you
				to round the width and height to next POT and so you know there
				will be pages that are never accessed.

				This memory is can to be exported and mapped into the device
				memory context of other processes, or to CPU.

                Size must be a positive integer multiple of the page size
@Input          psDevConnection     Device to allocation the memory for
@Input          uiSize              The logical size of allocation
@Input          uiChunkSize         The size of the chunk
@Input          ui32NumPhysChunks   The number of physical chunks required
@Input          ui32NumVirtChunks   The number of virtual chunks required
@Input			pui32MappingTable	index based Mapping table
@Input          uiLog2Align         Log2 of the required alignment
@Input          uiFlags             Allocation flags
@Input          pszText     		Text to describe the allocation
@Output         hMemDesc
@Return         PVRSRV_OK on success. Otherwise, a PVRSRV_ error code
*/ /***************************************************************************/
PVRSRV_ERROR
PVRSRVAllocSparseDevMem2(const PVRSRV_DEV_CONNECTION *psDevConnection,
						IMG_DEVMEM_SIZE_T uiSize,
						IMG_DEVMEM_SIZE_T uiChunkSize,
						IMG_UINT32 ui32NumPhysChunks,
						IMG_UINT32 ui32NumVirtChunks,
						IMG_UINT32 *pui32MappingTable,
						IMG_DEVMEM_LOG2ALIGN_T uiLog2Align,
						PVRSRV_MEMMAP_FLAGS_T uiFlags,
						IMG_PCHAR pszText,
						PVRSRV_MEMDESC *hMemDesc);

/**************************************************************************/ /*!
@Function       PVRSRVAllocSparseDevMem (DEPRECATED and will be removed in future)
@Description    Allocate sparse memory without mapping into device memory context.
				Sparse memory is used where you have an allocation that has a
				logical size (i.e. the amount of VM space it will need when
				mapping it into a device) that is larger then the amount of
				physical memory that allocation will use. An example of this
				is a NPOT texture where the twiddling algorithm requires you
				to round the width and height to next POT and so you know there
				will be pages that are never accessed.

				This memory is can to be exported and mapped into the device
				memory context of other processes, or to CPU.

                Size must be a positive integer multiple of the page size
                This function is deprecated and should not be used in any new code
                It will be removed in the subsequent changes.
@Input          psDevConnection     Device to allocation the memory for
@Input          uiSize              The logical size of allocation
@Input          uiChunkSize         The size of the chunk
@Input          ui32NumPhysChunks   The number of physical chunks required
@Input          ui32NumVirtChunks   The number of virtual chunks required
@Input			pabMappingTable		boolean based Mapping table
@Input          uiLog2Align         Log2 of the required alignment
@Input          uiFlags             Allocation flags
@Input          pszText     		Text to describe the allocation
@Output         hMemDesc
@Return         PVRSRV_OK on success. Otherwise, a PVRSRV_ error code
*/ /***************************************************************************/
PVRSRV_ERROR
PVRSRVAllocSparseDevMem(const PVRSRV_DEV_CONNECTION *psDevConnection,
						IMG_DEVMEM_SIZE_T uiSize,
						IMG_DEVMEM_SIZE_T uiChunkSize,
						IMG_UINT32 ui32NumPhysChunks,
						IMG_UINT32 ui32NumVirtChunks,
						IMG_BOOL *pabMappingTable,
						IMG_DEVMEM_LOG2ALIGN_T uiLog2Align,
						DEVMEM_FLAGS_T uiFlags,
						IMG_PCHAR pszText,
						PVRSRV_MEMDESC *hMemDesc);

/**************************************************************************/ /*!
@Function       PVRSRVGetLog2PageSize
@Description    Just call AFTER setting up the connection to the kernel module
                otherwise it will run into an assert.
                Gives the log2 of the page size that is currently utilised by
                devmem.

@Return         The page size
*/ /***************************************************************************/

IMG_UINT32 PVRSRVGetLog2PageSize(void);

/**************************************************************************/ /*!
@Function PVRSRVMakeLocalImportHandle
@Description    This is a "special case" function for making a local import
                handle. The server handle is a handle to a PMR of bridge type
                PMR_LOCAL_EXPORT_HANDLE. The returned local import handle will
                be of the bridge type DEVMEM_MEM_IMPORT that can be used with
                PVRSRVDevmemLocalImport().
@Input          psConnection        Services connection
@Input          hServerHandle       Server export handle
@Output         hLocalImportHandle  Returned client import handle
@Return         PVRSRV_ERROR:       PVRSRV_OK on success. Otherwise, a PVRSRV_
                                    error code
*/ /***************************************************************************/
PVRSRV_ERROR
PVRSRVMakeLocalImportHandle(const PVRSRV_DEV_CONNECTION *psConnection,
                            IMG_HANDLE hServerHandle,
                            IMG_HANDLE *hLocalImportHandle);

/**************************************************************************/ /*!
@Function PVRSRVUnmakeLocalImportHandle
@Description    Destroy the hLocalImportHandle created with
                PVRSRVMakeLocalImportHandle().
@Input          psConnection        Services connection
@Output         hLocalImportHandle  Local import handle
@Return         PVRSRV_ERROR:       PVRSRV_OK on success. Otherwise, a PVRSRV_
                                    error code
*/ /***************************************************************************/
PVRSRV_ERROR
PVRSRVUnmakeLocalImportHandle(const PVRSRV_DEV_CONNECTION *psConnection,
                              IMG_HANDLE hLocalImportHandle);

#if defined(SUPPORT_INSECURE_EXPORT)
/**************************************************************************/ /*!
@Function       PVRSRVExport
@Description    Given a memory allocation allocated with Devmem_Allocate(),
                create a "cookie" that can be passed intact by the caller's own
                choice of secure IPC to another process and used as the argument
                to "map" to map this memory into a heap in the target processes.
                N.B.  This can also be used to map into multiple heaps in one
                process, though that's not the intention.

                Note, the caller must later call Unexport before freeing the
                memory.
@Input          hMemDesc        handle to the descriptor of the memory to be
                                exported
@Output         phExportCookie  On success, a handle to the exported cookie
@Return         PVRSRV_ERROR:   PVRSRV_OK on success. Otherwise, a PVRSRV_
                                error code
*/ /***************************************************************************/
PVRSRV_ERROR PVRSRVExportDevMem(PVRSRV_MEMDESC hMemDesc,
						  		PVRSRV_DEVMEM_EXPORTCOOKIE *phExportCookie);

/**************************************************************************/ /*!
@Function       PVRSRVUnexport
@Description    Undo the export caused by "PVRSRVExport" - note - it doesn't
                actually tear down any mapping made by processes that received
                the export cookie.  It will simply make the cookie null and void
                and prevent further mappings.
@Input          hMemDesc        handle to the descriptor of the memory which
                                will no longer be exported
@Output         phExportCookie  On success, the export cookie provided will be
                                set to null
@Return         PVRSRV_ERROR:   PVRSRV_OK on success. Otherwise, a PVRSRV_
                                error code
*/ /***************************************************************************/
PVRSRV_ERROR PVRSRVUnexportDevMem(PVRSRV_MEMDESC hMemDesc,
								  PVRSRV_DEVMEM_EXPORTCOOKIE *phExportCookie);

/**************************************************************************/ /*!
@Function       PVRSRVImportDevMem
@Description    Import memory that was previously exported with PVRSRVExport()
                into the current process.

                Note: This call only makes the memory accessible to this
                process, it doesn't map it into the device or CPU.

@Input          psConnection    Connection to services
@Input          phExportCookie  Ptr to the handle of the export-cookie 
                                identifying                          
@Output         phMemDescOut    On Success, a handle to a new memory descriptor
                                representing the memory as mapped into the
                                local process address space.
@Input          uiFlags         Device memory mapping flags                                
@Input          pszText     	Text to describe the import
@Return         PVRSRV_ERROR:   PVRSRV_OK on success. Otherwise, a PVRSRV_
                                error code
*/ /***************************************************************************/
PVRSRV_ERROR PVRSRVImportDevMem(const PVRSRV_DEV_CONNECTION *psConnection,
								PVRSRV_DEVMEM_EXPORTCOOKIE *phExportCookie,
								PVRSRV_MEMMAP_FLAGS_T uiFlags,
								PVRSRV_MEMDESC *phMemDescOut);
#endif /* SUPPORT_INSECURE_EXPORT */

/**************************************************************************/ /*!
@Function       PVRSRVIsDeviceMemAddrValid
@Description    Checks if given device virtual memory address is valid
                from the GPU's point of view.
@Input          hContext handle to memory context
@Input          sDevVAddr device 40bit virtual memory address
@Return         PVRSRV_OK if address is valid or
                PVRSRV_ERROR_INVALID_GPU_ADDR when address is invalid
*/ /***************************************************************************/
PVRSRV_ERROR PVRSRVIsDeviceMemAddrValid(PVRSRV_DEVMEMCTX hContext,
                                        IMG_DEV_VIRTADDR sDevVAddr);


/**************************************************************************/ /*!
@Function       PVRSRVDevmemPin
@Description    This is the counterpart to PVRSRVDevmemUnpin. It is meant to be
                called after unpinning an allocation.

                It will make an unpinned allocation available again and
                unregister it from the OS shrinker. In the case the shrinker
                was invoked by the OS while the allocation was unpinned it will
                allocate new physical pages.

                If any GPU mapping existed before, the same virtual address
                range will be valid again.

@Input          hMemDesc        The MemDesc that is going to be pinned.

@Return         PVRSRV_ERROR:   PVRSRV_OK on success and the pre-unpin content
                                is still present and can be reused.

                                PVRSRV_ERROR_PMR_NEW_MEMORY if the memory has
                                been pinned successfully but the pre-unpin
                                content was lost.

                                PVRSRV_ERROR_INVALID_PARAMS if the MemDesc is
                                invalid e.g. NULL.

                                PVRSRV_ERROR_PMR_FAILED_TO_ALLOC_PAGES if the
                                memory of the allocation is lost and we failed
                                to allocate new one.
*/ /***************************************************************************/
extern PVRSRV_ERROR
PVRSRVDevmemPin(PVRSRV_MEMDESC hMemDesc);

/**************************************************************************/ /*!
@Function       PVRSRVDevmemUnpin
@Description    Unpins an allocation. Unpinning means that the
                memory must not be accessed anymore by neither CPU nor GPU.
                The physical memory pages will be registered with a shrinker
                and the OS is able to reclaim them in OOM situations when the
                shrinker is invoked.

                The counterpart to this is PVRSRVDevmemPin() which
                checks if the physical pages were reclaimed by the OS and then
                either allocates new physical pages or just unregisters the
                allocation from the shrinker. The device virtual address range
                (if any existed) will be kept.

                The GPU mapping will be kept but is going be invalidated.
                It is allowed to free an unpinned allocation or remove the GPU
                mapping.

                RESTRICTIONS:
                - Unpinning should only be done if the caller is sure that
                the GPU finished all pending/running operations on the allocation.

                - The caller must ensure that no other process than the calling
                one itself has imported or mapped the allocation, otherwise the
                unpinning will fail.

                - All CPU mappings have to be removed beforehand by the caller.

                - Any attempts to map the allocation while it is unpinned are
                forbidden.

                - When using PVRSRVAllocDeviceMem() the caller must allocate
                whole pages from the choosen heap to avoid suballocations.

@Input          hMemDesc       The MemDesc that is going to be unpinned.

@Return         PVRSRV_ERROR:   PVRSRV_OK on success.

                                PVRSRV_ERROR_NOT_SUPPORTED if the PMR
                                factory does not allow unpinning.

                                PVRSRV_ERROR_INVALID_PARAMS if the passed
                                allocation is not a multiple of the heap page
                                size but was allocated with
                                PVRSRVAllocDeviceMem(), or if its NULL.

                                PVRSRV_ERROR_PMR_STILL_REFERENCED if the passed
                                allocation is still referenced i.e. is still
                                exported or mapped somewhere else.

                                PVRSRV_ERROR_STILL_MAPPED will be thrown if the
                                calling process still has CPU mappings set up
                                or the GPU mapping was acquired more than once.
*/ /***************************************************************************/
extern PVRSRV_ERROR
PVRSRVDevmemUnpin(PVRSRV_MEMDESC hMemDesc);

#if defined (SUPPORT_EXPORTING_MEMORY_CONTEXT)
/**************************************************************************/ /*!
@Function       PVRSRVExportDevmemContext
@Description    Export a device memory context to another process

@Input          hCtx            Memory context to export                        
@Output         phExport        On Success, a export handle that can be passed
                                to another process and used with 
                                PVRSRVImportDeviceMemContext to import the
                                memory context                            
@Return         PVRSRV_ERROR:   PVRSRV_OK on success. Otherwise, a PVRSRV_
                                error code
*/ /***************************************************************************/
PVRSRV_ERROR
PVRSRVExportDevmemContext(PVRSRV_DEVMEMCTX hCtx,
						  IMG_HANDLE *phExport);

/**************************************************************************/ /*!
@Function       PVRSRVUnexportDevmemContext
@Description    Unexport an exported device memory context

@Input          psConnection    Services connection
@Input          hExport         Export handle created to be unexported

@Return         None
*/ /***************************************************************************/
void
PVRSRVUnexportDevmemContext(PVRSRV_DEV_CONNECTION *psConnection,
							IMG_HANDLE hExport);

/**************************************************************************/ /*!
@Function       PVRSRVImportDeviceMemContext
@Description    Import an exported device memory context

                Note: The memory context created with this function is not
                complete and can only be used with debugger related functions

@Input          psConnection    Services connection
@Input          hExport         Export handle to import
@Output         phCtxOut        Device memory context

@Return         None
*/ /***************************************************************************/
PVRSRV_ERROR
PVRSRVImportDeviceMemContext(PVRSRV_DEV_CONNECTION *psConnection,
							 IMG_HANDLE hExport,
							 PVRSRV_DEVMEMCTX *phCtxOut);

#endif /* SUPPORT_EXPORTING_MEMORY_CONTEXT */
#if defined __cplusplus
};
#endif
#endif /* PVRSRV_DEVMEM_H */

