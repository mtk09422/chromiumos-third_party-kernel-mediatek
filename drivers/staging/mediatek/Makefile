mediatek-drm-objs := mediatek_drm_drv.o \
                  mediatek_drm_crtc.o \
                  mediatek_drm_fb.o \
                  mediatek_drm_output.o \
                  mediatek_drm_gem.o \
                  mediatek_drm_dsi.o \
                  it6151.o \
                  mediatek_drm_hw.o

obj-$(CONFIG_DRM_MEDIATEK) += mediatek-drm.o

ifneq ($(CONFIG_DRM_MEDIATEK_ROGUE),)
rogue_basedir := drivers/staging/img-rogue
dc_basedir   := drivers/staging/mediatek

include $(rogue_basedir)/config_kernel.mk

obj-$(CONFIG_DRM_MEDIATEK_ROGUE) += $(DISPLAY_CONTROLLER).o

ccflags-y += \
 -include $(KERNEL_DIR)/$(rogue_basedir)/config_kernel.h \
 -I$(rogue_basedir) \
 -I$(dc_basedir)

ccflags-y += -DPVRDRM

include $(dc_basedir)/dc_drmmtk.mk
endif
