override SUPPORT_COMPUTE := 1
override PVR_HANDLE_BACKEND := idr
override CACHEFLUSH_TYPE := CACHEFLUSH_GENERIC
override PVR_LOADER := mtk_module
override SUPPORT_INSECURE_EXPORT := 1
override SUPPORT_SECURE_EXPORT := 1
override SUPPORT_SERVER_SYNC := 1
override PVRSRV_MODNAME := pvrsrvkm
override SUPPORT_DRM_DC_MODULE := 1
override SUPPORT_DRM := 1
override DISPLAY_CONTROLLER := dc_drmmtk
override PVR_BUILD_DIR := mt8173_linux
override PVR_SYSTEM := mt8173
ifeq ($(CONFIG_POWERVR_ROGUE_DEBUG),y)
override PVR_RI_DEBUG := 1
override PVR_BUILD_TYPE := debug
override SUPPORT_PAGE_FAULT_DEBUG := 1
override BUILD := debug
endif
ifneq ($(CONFIG_POWERVR_ROGUE_DEBUG),y)
override BUILD := release
override PVR_BUILD_TYPE := release
endif
