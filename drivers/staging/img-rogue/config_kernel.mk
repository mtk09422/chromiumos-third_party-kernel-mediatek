override PVR_SYSTEM := rgx_mtk
override SUPPORT_DRM := 1
override PVR_HANDLE_BACKEND := idr
override CACHEFLUSH_TYPE := CACHEFLUSH_GENERIC
override SUPPORT_INSECURE_EXPORT := 1
override SUPPORT_SECURE_EXPORT := 1
override PVRSRV_MODNAME := pvrsrvkm
override SUPPORT_DRM_DC_MODULE := 1
override DISPLAY_CONTROLLER := dc_drmmtk
ifeq ($(CONFIG_POWERVR_ROGUE_DEBUG),y)
override PVR_RI_DEBUG := 1
override BUILD := debug
override PVR_BUILD_TYPE := debug
endif
ifneq ($(CONFIG_POWERVR_ROGUE_DEBUG),y)
override BUILD := release
override PVR_BUILD_TYPE := release
endif
ifeq ($(CONFIG_POWERVR_ROGUE_XT),y)
override PVR_BUILD_DIR := mt8173_linux
endif
ifneq ($(CONFIG_POWERVR_ROGUE_XT),y)
override PVR_BUILD_DIR := mt8135_linux
endif
