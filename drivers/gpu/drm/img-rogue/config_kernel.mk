override SUPPORT_COMPUTE := 1
override CACHEFLUSH_TYPE := CACHEFLUSH_GENERIC
override SUPPORT_DRM := 1
override SUPPORT_SECURE_EXPORT := 1
override SUPPORT_SERVER_SYNC := 1
override PVR_BUILD_DIR := mt8173_linux
override PVRSRV_MODNAME := pvrsrvkm
override PVR_HANDLE_BACKEND := idr
override PVR_SYSTEM := mt8173
ifeq ($(CONFIG_POWERVR_ROGUE_DEBUG),y)
override PVR_RI_DEBUG := 1
override PVR_BUILD_TYPE := debug
override BUILD := debug
override SUPPORT_PAGE_FAULT_DEBUG := 1
endif
ifneq ($(CONFIG_POWERVR_ROGUE_DEBUG),y)
override PVR_BUILD_TYPE := release
override BUILD := release
endif
