mediatek-drm-objs := mediatek_drm_drv.o \
                  mediatek_drm_crtc.o \
                  mediatek_drm_fb.o \
                  mediatek_drm_gem.o \
                  mediatek_drm_dmabuf.o \
                  mediatek_drm_dsi.o \
                  mediatek_drm_debugfs.o \
                  mediatek_drm_ddp.o

obj-$(CONFIG_DRM_MEDIATEK) += mediatek-drm.o

ccflags-y += \
 -Idrivers/gpu/drm

mediatek-drm-hdmi-objs := mediatek_hdmi_display_core.o \
                  mediatek_drm_hdmi_drv.o \
                  mediatek_hdmi_ddc_drv.o \
                  mediatek_hdmi_dpi_drv.o \
                  mediatek_hdmi_dpi_ctrl.o \
                  mediatek_hdmi_dpi_hw.o \
                  mediatek_hdmi_ctrl.o \
                  mediatek_hdmi_debugfs.o \
                  mediatek_hdmi_hw.o

obj-$(CONFIG_DRM_MEDIATEK_HDMI) += mediatek-drm-hdmi.o

