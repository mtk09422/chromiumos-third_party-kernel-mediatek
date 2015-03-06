mediatek-drm-objs := mediatek_drm_drv.o \
                  mediatek_drm_crtc.o \
                  mediatek_drm_fb.o \
                  mediatek_drm_output.o \
                  mediatek_drm_gem.o \
                  mediatek_drm_dmabuf.o \
                  mediatek_drm_dsi.o \
                  it6151.o \
                  mediatek_drm_debugfs.o \
                  mediatek_drm_ddp.o

obj-$(CONFIG_DRM_MEDIATEK) += mediatek-drm.o

ifneq ($(CONFIG_DRM_MEDIATEK_ROGUE),)
rogue_basedir := drivers/gpu/drm/img-rogue
dc_basedir   := drivers/gpu/drm/mediatek


include $(rogue_basedir)/config_kernel.mk
ccflags-y += -include ../../../../$(rogue_basedir)/config_kernel.h


obj-$(CONFIG_DRM_MEDIATEK_ROGUE) += $(DISPLAY_CONTROLLER).o

ccflags-y += \
 -I$(rogue_basedir) \
 -I$(dc_basedir) \
 -Idrivers/gpu/drm

ccflags-y += -DPVRDRM

$(DISPLAY_CONTROLLER)-y += \
 dc_mediatek_drm_drv.o \
 mediatek_drm_crtc.o \
 mediatek_drm_fb.o \
 mediatek_drm_output.o \
 mediatek_drm_ddp.o \
 mediatek_drm_debugfs.o \
 mediatek_drm_dsi.o \
 it6151.o

#include $(dc_basedir)/dc_drmmtk.mk
endif
