########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Strictly Confidential.
### ###########################################################################
img_basedir := drivers/staging/img-rogue
include $(img_basedir)/config_kernel.mk

obj-$(CONFIG_POWERVR_ROGUE) += pvrsrvkm.o

ccflags-y += \
 -include config_kernel.h \
 -I$(img_basedir) \
 -I$(img_basedir)/km \
 -D__linux__


include $(img_basedir)/pvrsrvkm.mk

