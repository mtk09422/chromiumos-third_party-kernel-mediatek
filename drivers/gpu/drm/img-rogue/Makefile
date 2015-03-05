img_basedir := drivers/gpu/drm/img-rogue
include $(img_basedir)/config_kernel.mk

obj-$(CONFIG_POWERVR_ROGUE) += pvrsrvkm.o

ccflags-y += \
 -include config_kernel.h \
 -I$(img_basedir) \
 -I$(img_basedir)/include \
 -I$(img_basedir)/km \
 -D__linux__

ifeq ($(CONFIG_POWERVR_ROGUE_MT8135),y)
ccflags-y += -I$(img_basedir)/mt8135
else
ccflags-y += -I$(img_basedir)/mt8173
endif

include $(img_basedir)/pvrsrvkm.mk

